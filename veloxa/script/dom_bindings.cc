#include "veloxa/script/dom_bindings.h"

extern "C" {
#include "quickjs.h"
}

#include <cstdio>
#include <cstring>

#include "veloxa/core/css/enum_serialization.h"
#include "veloxa/core/css/parser.h"
#include "veloxa/core/css/property.h"
#include "veloxa/core/dom/text.h"
#include "veloxa/foundation/strings/interned_string.h"
#include "veloxa/foundation/strings/string.h"

namespace vx::script {

// InstanceData is the per-DomBindings state formerly held in file-scope
// globals. It is accessed from free-function JS callbacks via
// JS_GetContextOpaque(ctx) → DomBindings* → data_.
struct DomBindings::InstanceData {
  JSContext* ctx = nullptr;
  dom::Document* doc = nullptr;
  // TASK-20260502-01 A.1.4 — target Document inspected by DevTool dogfood
  // UI's vx_devtool_get_dom_json() native binding (see RegisterDevtoolBindings
  // / DomBindings::SetDevtoolTargetDocument). nullptr is the unset state
  // (VxDevtoolGetDomJson returns the JSON string "null").
  dom::Document* devtool_target_doc = nullptr;
  // B.2.2 — PerfOverlay live FrameStats source (borrowed; nullptr when not
  // attached). vx_view_get_perf_stats binding reads aggregated() + fps().
  vx::devtool::overlay::PerfOverlay* perf_overlay = nullptr;
  // C.3.1 — HotReloadManager status source (borrowed; nullptr when not
  // attached). vx_devtool_get_hot_reload_status binding reads
  // tracked_count() + last_error().
  vx::devtool::hot_reload::HotReloadManager* hot_reload_manager = nullptr;
  // A.2.1 T3 mitigation: redaction policy applied by VxDevtoolGetDomJson
  // when serializing the target Document. Defaults to the safe value;
  // synced from Application::redaction_policy() via SetRedactionPolicy.
  dom::RedactionPolicy redaction_policy =
      dom::RedactionPolicy::kRedactSensitive;
  event::EventManager* em = nullptr;
  // Token returned by em->AddDestructionObserver in Bind. Used in two places:
  //   - Unbind (when em is still alive) calls em->RemoveDestructionObserver
  //     to deregister our callback before this InstanceData goes out of scope.
  //   - The observer itself fires when em is destroyed first; it nulls em
  //     and clears listener_elements so Unbind becomes a no-op for them.
  // 0 means "no observer registered".
  event::EventManager::DestructionObserverToken em_observer_token = 0;
  // Elements that received at least one addEventListener. Used by Unbind
  // to deterministically tear down lambdas before freeing JS callbacks.
  // (See #50 fix in Phase 2.)
  Vector<dom::Element*> listener_elements;

  // B7 (TASK-20260419-01): per-(element, type, JS callback identity) record of
  // the ListenerToken that EventManager handed back. Used by
  // ElementRemoveEventListener(type, handler) to remove exactly one listener
  // by token instead of the legacy bulk RemoveEventListeners.
  // callback_ptr is JS_VALUE_GET_PTR of the registered callback; identity is
  // pointer equality (matches DOM `removeEventListener` "same-object" semantic
  // for object handlers).
  struct JsListenerEntry {
    dom::Element* element;
    event::EventType type;
    void* callback_ptr;
    event::ListenerToken token;
  };
  Vector<JsListenerEntry> js_listener_entries;

  struct TrackedCallbacks {
    JSContext* ctx = nullptr;
    Vector<JSValue> callbacks;

    void Track(JSValue cb) { callbacks.push_back(cb); }
    void FreeAll() {
      if (!ctx) return;
      for (usize i = 0; i < callbacks.size(); ++i) {
        JS_FreeValue(ctx, callbacks[i]);
      }
      callbacks.clear();
      ctx = nullptr;
    }
  } tracked_callbacks;
};

// Bridge for free-function callbacks to access InstanceData without
// widening the public API surface of DomBindings.
struct DomBindingsInternal {
  static DomBindings::InstanceData* Data(DomBindings* b) {
    return b ? b->data_.get() : nullptr;
  }
};

namespace {

DomBindings* GetBindings(JSContext* ctx) {
  return static_cast<DomBindings*>(JS_GetContextOpaque(ctx));
}

DomBindings::InstanceData* GetData(JSContext* ctx) {
  return DomBindingsInternal::Data(GetBindings(ctx));
}

// ----- JS event name → EventType mapping -----

bool MapJsEventName(const char* name, event::EventType* out) {
  struct Mapping {
    const char* js_name;
    event::EventType type;
  };
  static const Mapping kMappings[] = {
      {"pointerdown", event::EventType::kPointerDown},
      {"pointerup", event::EventType::kPointerUp},
      {"pointermove", event::EventType::kPointerMove},
      {"keydown", event::EventType::kKeyDown},
      {"keyup", event::EventType::kKeyUp},
      {"touchstart", event::EventType::kTouchStart},
      {"touchend", event::EventType::kTouchEnd},
      {"touchmove", event::EventType::kTouchMove},
      {"focusin", event::EventType::kFocusIn},
      {"focusout", event::EventType::kFocusOut},
  };
  for (const auto& m : kMappings) {
    if (std::strcmp(name, m.js_name) == 0) {
      *out = m.type;
      return true;
    }
  }
  return false;
}

const char* EventTypeToString(event::EventType type) {
  switch (type) {
    case event::EventType::kPointerDown:
      return "pointerdown";
    case event::EventType::kPointerUp:
      return "pointerup";
    case event::EventType::kPointerMove:
      return "pointermove";
    case event::EventType::kKeyDown:
      return "keydown";
    case event::EventType::kKeyUp:
      return "keyup";
    case event::EventType::kTouchStart:
      return "touchstart";
    case event::EventType::kTouchEnd:
      return "touchend";
    case event::EventType::kTouchMove:
      return "touchmove";
    case event::EventType::kFocusIn:
      return "focusin";
    case event::EventType::kFocusOut:
      return "focusout";
  }
  return "unknown";
}

// ----- Element JS class -----
//
// JSClassID values are process-wide identifiers managed by QuickJS's
// JSRuntime class table. They are not per-DomBindings state: multiple
// runtimes reuse the same numeric id, and a fresh runtime starts with
// that id unregistered. We keep them as file-scope statics and register
// idempotently via JS_IsRegisteredClass so repeated Bind calls within a
// single runtime do not re-register the class.

static JSClassID s_element_class_id = 0;
static JSClassID s_style_class_id = 0;

dom::Element* GetElement(JSContext* /*ctx*/, JSValueConst this_val) {
  return static_cast<dom::Element*>(
      JS_GetOpaque(this_val, s_element_class_id));
}

void ElementFinalizer(JSRuntime* /*rt*/, JSValueConst /*val*/) {}

JSClassDef g_element_class_def = {
    "Element",
    ElementFinalizer,
    nullptr,
    nullptr,
    nullptr,
};

// ----- Style proxy JS class -----

struct StyleOpaque {
  dom::Element* element;
};

StyleOpaque* GetStyleOpaque(JSValueConst this_val) {
  return static_cast<StyleOpaque*>(
      JS_GetOpaque(this_val, s_style_class_id));
}

void StyleFinalizer(JSRuntime* /*rt*/, JSValueConst val) {
  auto* so = static_cast<StyleOpaque*>(
      JS_GetOpaque(val, s_style_class_id));
  delete so;
}

JSClassDef g_style_class_def = {
    "CSSStyleDeclaration",
    StyleFinalizer,
    nullptr,
    nullptr,
    nullptr,
};

// ----- camelCase → kebab-case + PropertyId mapping -----

struct CamelMapping {
  const char* camel;
  const char* kebab;
  css::PropertyId prop;
};

static const CamelMapping kStyleMappings[] = {
    {"backgroundColor", "background-color", css::PropertyId::kBackgroundColor},
    {"color", "color", css::PropertyId::kColor},
    {"width", "width", css::PropertyId::kWidth},
    {"height", "height", css::PropertyId::kHeight},
    {"display", "display", css::PropertyId::kDisplay},
    {"opacity", "opacity", css::PropertyId::kOpacity},
    {"fontSize", "font-size", css::PropertyId::kFontSize},
};

static constexpr int kStyleMappingCount =
    static_cast<int>(sizeof(kStyleMappings) / sizeof(kStyleMappings[0]));

// ----- CSS value → string serialization (read path for StyleGetProp) -----

const char* UnitSuffix(css::Unit u) {
  switch (u) {
    case css::Unit::kPx: return "px";
    case css::Unit::kEm: return "em";
    case css::Unit::kRem: return "rem";
    case css::Unit::kPercent: return "%";
    case css::Unit::kVw: return "vw";
    case css::Unit::kVh: return "vh";
    case css::Unit::kAuto:
    case css::Unit::kNumber:
    case css::Unit::kNone:
      return "";
  }
  return "";
}

void AppendCStr(const char* s, String* out) {
  out->append(StringView(s));
}

void AppendNumber(f32 v, String* out) {
  char buf[32];
  int n = std::snprintf(buf, sizeof(buf), "%g", static_cast<double>(v));
  if (n > 0) out->append(StringView(buf, static_cast<usize>(n)));
}

void SerializeCssValue(const css::CssValue& v, css::PropertyId prop,
                       String* out) {
  switch (v.type) {
    case css::ValueType::kLength:
      AppendNumber(v.number, out);
      AppendCStr(UnitSuffix(v.unit), out);
      break;
    case css::ValueType::kColor: {
      u32 rgba = v.color;
      u8 r = static_cast<u8>((rgba >> 24) & 0xFF);
      u8 g = static_cast<u8>((rgba >> 16) & 0xFF);
      u8 b = static_cast<u8>((rgba >> 8) & 0xFF);
      u8 a = static_cast<u8>(rgba & 0xFF);
      char buf[48];
      int n = std::snprintf(buf, sizeof(buf),
                            "rgba(%u, %u, %u, %u)", r, g, b, a);
      if (n > 0) out->append(StringView(buf, static_cast<usize>(n)));
      break;
    }
    case css::ValueType::kNumber:
      AppendNumber(v.number, out);
      break;
    case css::ValueType::kAuto:
      AppendCStr("auto", out);
      break;
    case css::ValueType::kInherit:
      AppendCStr("inherit", out);
      break;
    case css::ValueType::kInitial:
      AppendCStr("initial", out);
      break;
    case css::ValueType::kEnum: {
      StringView s = css::EnumValueToCssString(prop, v.enum_value);
      if (!s.empty()) out->append(s);
      break;
    }
    case css::ValueType::kNone:
      break;
  }
}

// ----- Style property magic getter/setter -----

JSValue StyleGetProp(JSContext* ctx, JSValueConst this_val, int magic) {
  auto* so = GetStyleOpaque(this_val);
  if (!so || !so->element || magic < 0 || magic >= kStyleMappingCount) {
    return JS_NewString(ctx, "");
  }

  const auto* decls = so->element->inline_declarations();
  if (!decls) return JS_NewString(ctx, "");

  css::PropertyId target = kStyleMappings[magic].prop;
  for (usize i = 0; i < decls->size(); ++i) {
    const auto& d = (*decls)[i];
    if (d.property == target) {
      String out;
      SerializeCssValue(d.value, target, &out);
      return JS_NewStringLen(ctx, out.data(), out.size());
    }
  }
  return JS_NewString(ctx, "");
}

JSValue StyleSetProp(JSContext* ctx, JSValueConst this_val, JSValueConst val,
                     int magic) {
  auto* so = GetStyleOpaque(this_val);
  if (!so || !so->element || magic < 0 || magic >= kStyleMappingCount) {
    return JS_UNDEFINED;
  }

  const char* val_str = JS_ToCString(ctx, val);
  if (!val_str) return JS_UNDEFINED;

  const CamelMapping& mapping = kStyleMappings[magic];
  String css_text(mapping.kebab);
  css_text += ':';
  css_text += val_str;
  JS_FreeCString(ctx, val_str);

  auto decls = css::CssParser::ParseDeclarationList(css_text.view());
  for (usize i = 0; i < decls.size(); ++i) {
    so->element->SetInlineDeclaration(decls[i].property, decls[i].value);
  }

  return JS_UNDEFINED;
}

// ----- Element methods -----

JSValue ElementGetAttribute(JSContext* ctx, JSValueConst this_val, int argc,
                            JSValueConst* argv) {
  auto* el = GetElement(ctx, this_val);
  if (!el || argc < 1) return JS_NULL;

  const char* attr_name = JS_ToCString(ctx, argv[0]);
  if (!attr_name) return JS_NULL;

  auto interned = InternedString::Intern(attr_name);
  JS_FreeCString(ctx, attr_name);

  const String* val = el->GetAttribute(interned);
  if (!val) return JS_NULL;
  return JS_NewStringLen(ctx, val->data(), val->size());
}

JSValue ElementSetAttribute(JSContext* ctx, JSValueConst this_val, int argc,
                            JSValueConst* argv) {
  auto* el = GetElement(ctx, this_val);
  if (!el || argc < 2) return JS_UNDEFINED;

  const char* attr_name = JS_ToCString(ctx, argv[0]);
  if (!attr_name) return JS_UNDEFINED;
  auto interned = InternedString::Intern(attr_name);
  JS_FreeCString(ctx, attr_name);

  const char* attr_val = JS_ToCString(ctx, argv[1]);
  if (!attr_val) return JS_UNDEFINED;
  el->SetAttribute(interned, String(attr_val));
  JS_FreeCString(ctx, attr_val);

  return JS_UNDEFINED;
}

// ----- Element property getters/setters -----

JSValue ElementGetTagName(JSContext* ctx, JSValueConst this_val) {
  auto* el = GetElement(ctx, this_val);
  if (!el) return JS_UNDEFINED;
  return JS_NewString(ctx, el->tag_name());
}

JSValue ElementGetId(JSContext* ctx, JSValueConst this_val) {
  auto* el = GetElement(ctx, this_val);
  if (!el) return JS_UNDEFINED;
  auto id = el->id();
  if (id.empty()) return JS_NewString(ctx, "");
  return JS_NewStringLen(ctx, id.data(), id.size());
}

JSValue ElementGetTextContent(JSContext* ctx, JSValueConst this_val) {
  auto* el = GetElement(ctx, this_val);
  if (!el) return JS_UNDEFINED;

  for (dom::Node* child = el->first_child(); child;
       child = child->next_sibling()) {
    if (child->is_text()) {
      auto* text = static_cast<dom::Text*>(child);
      return JS_NewStringLen(ctx, text->data().data(), text->data().size());
    }
  }
  return JS_NewString(ctx, "");
}

JSValue ElementSetTextContent(JSContext* ctx, JSValueConst this_val,
                              JSValueConst val) {
  auto* el = GetElement(ctx, this_val);
  if (!el) return JS_UNDEFINED;

  const char* str = JS_ToCString(ctx, val);
  if (!str) return JS_UNDEFINED;

  for (dom::Node* child = el->first_child(); child;
       child = child->next_sibling()) {
    if (child->is_text()) {
      auto* text = static_cast<dom::Text*>(child);
      text->set_data(String(str));
      JS_FreeCString(ctx, str);
      return JS_UNDEFINED;
    }
  }

  JS_FreeCString(ctx, str);
  return JS_UNDEFINED;
}

// ----- addEventListener / removeEventListener -----

JSValue ElementAddEventListener(JSContext* ctx, JSValueConst this_val, int argc,
                                JSValueConst* argv) {
  auto* el = GetElement(ctx, this_val);
  if (!el || argc < 2) return JS_UNDEFINED;

  const char* type_str = JS_ToCString(ctx, argv[0]);
  if (!type_str) return JS_UNDEFINED;

  event::EventType event_type;
  bool valid = MapJsEventName(type_str, &event_type);
  JS_FreeCString(ctx, type_str);
  if (!valid) return JS_UNDEFINED;

  JSValue callback = JS_DupValue(ctx, argv[1]);
  auto* data = GetData(ctx);
  if (!data || !data->em) {
    JS_FreeValue(ctx, callback);
    return JS_UNDEFINED;
  }

  data->tracked_callbacks.Track(callback);

  // Record element once so Unbind can tear down its listeners before
  // the JS callbacks are freed (see #50 in Phase 2).
  bool found = false;
  for (usize i = 0; i < data->listener_elements.size(); ++i) {
    if (data->listener_elements[i] == el) {
      found = true;
      break;
    }
  }
  if (!found) data->listener_elements.push_back(el);

  JSContext* captured_ctx = ctx;
  event::ListenerToken token = data->em->AddEventListener(
      el, event_type,
      [captured_ctx, callback](event::DOMEvent& event) mutable {
        JSValue js_event = JS_NewObject(captured_ctx);
        JS_SetPropertyStr(captured_ctx, js_event, "type",
                          JS_NewString(captured_ctx,
                                       EventTypeToString(event.input.type)));
        JSValue ret =
            JS_Call(captured_ctx, callback, JS_UNDEFINED, 1, &js_event);
        JS_FreeValue(captured_ctx, ret);
        JS_FreeValue(captured_ctx, js_event);
      });

  // Record (element, type, callback_ptr) → token so removeEventListener can
  // target this specific listener by JS handler identity.
  data->js_listener_entries.push_back(DomBindings::InstanceData::JsListenerEntry{
      el, event_type, JS_VALUE_GET_PTR(callback), token});

  return JS_UNDEFINED;
}

// Internal helper: drop entry at index i from js_listener_entries (swap-pop).
static void DropJsEntry(DomBindings::InstanceData* data, usize i) {
  usize last = data->js_listener_entries.size() - 1;
  if (i != last) {
    data->js_listener_entries[i] = data->js_listener_entries[last];
  }
  data->js_listener_entries.pop_back();
}

// Internal helper: if `el` no longer has any tracked entries, drop it from
// listener_elements so Unbind doesn't redundantly bulk-remove it.
static void MaybeForgetElement(DomBindings::InstanceData* data,
                               dom::Element* el) {
  for (usize i = 0; i < data->js_listener_entries.size(); ++i) {
    if (data->js_listener_entries[i].element == el) return;  // still tracked
  }
  for (usize i = 0; i < data->listener_elements.size(); ++i) {
    if (data->listener_elements[i] == el) {
      usize last = data->listener_elements.size() - 1;
      if (i != last) data->listener_elements[i] = data->listener_elements[last];
      data->listener_elements.pop_back();
      return;
    }
  }
}

JSValue ElementRemoveEventListener(JSContext* ctx, JSValueConst this_val,
                                   int argc, JSValueConst* argv) {
  auto* el = GetElement(ctx, this_val);
  auto* data = GetData(ctx);
  if (!el || !data || !data->em) return JS_UNDEFINED;

  // Two supported call shapes:
  //   removeEventListener(type)            — bulk remove (legacy 1-arg form)
  //   removeEventListener(type, handler)   — precise remove by JS handler
  // Anything with no args clears every listener on the element (very legacy).
  if (argc == 0) {
    data->em->RemoveEventListeners(el);
    for (usize i = data->js_listener_entries.size(); i-- > 0;) {
      if (data->js_listener_entries[i].element == el) DropJsEntry(data, i);
    }
    MaybeForgetElement(data, el);
    return JS_UNDEFINED;
  }

  const char* type_str = JS_ToCString(ctx, argv[0]);
  if (!type_str) return JS_UNDEFINED;
  event::EventType event_type;
  bool valid = MapJsEventName(type_str, &event_type);
  JS_FreeCString(ctx, type_str);
  if (!valid) return JS_UNDEFINED;

  if (argc < 2 || JS_IsUndefined(argv[1]) || JS_IsNull(argv[1])) {
    // 1-arg legacy form: remove every listener on `el` of `event_type`.
    for (usize i = data->js_listener_entries.size(); i-- > 0;) {
      auto& e = data->js_listener_entries[i];
      if (e.element == el && e.type == event_type) {
        data->em->RemoveEventListenerByToken(el, e.token);
        DropJsEntry(data, i);
      }
    }
    MaybeForgetElement(data, el);
    return JS_UNDEFINED;
  }

  // 2-arg precise form: match JS handler identity (pointer equality).
  void* handler_ptr = JS_VALUE_GET_PTR(argv[1]);
  for (usize i = 0; i < data->js_listener_entries.size(); ++i) {
    auto& e = data->js_listener_entries[i];
    if (e.element == el && e.type == event_type &&
        e.callback_ptr == handler_ptr) {
      data->em->RemoveEventListenerByToken(el, e.token);
      DropJsEntry(data, i);
      MaybeForgetElement(data, el);
      return JS_UNDEFINED;
    }
  }
  // Unknown (type, handler) pair: silent no-op (matches DOM spec).
  return JS_UNDEFINED;
}

JSValue ElementGetStyle(JSContext* ctx, JSValueConst this_val) {
  auto* el = GetElement(ctx, this_val);
  if (!el) return JS_UNDEFINED;

  JSValue style_obj =
      JS_NewObjectClass(ctx, static_cast<int>(s_style_class_id));
  if (JS_IsException(style_obj)) return style_obj;

  auto* so = new StyleOpaque{el};
  JS_SetOpaque(style_obj, so);
  return style_obj;
}

// ----- Helper to create getter/setter from typed function pointers -----

JSValue MakeGetter(JSContext* ctx,
                   JSValue (*fn)(JSContext*, JSValueConst),
                   const char* name) {
  JSCFunctionType ft;
  ft.getter = fn;
  return JS_NewCFunction2(ctx, ft.generic, name, 0, JS_CFUNC_getter, 0);
}

JSValue MakeSetter(JSContext* ctx,
                   JSValue (*fn)(JSContext*, JSValueConst, JSValueConst),
                   const char* name) {
  JSCFunctionType ft;
  ft.setter = fn;
  return JS_NewCFunction2(ctx, ft.generic, name, 1, JS_CFUNC_setter, 0);
}

JSValue MakeGetterMagic(
    JSContext* ctx,
    JSValue (*fn)(JSContext*, JSValueConst, int),
    const char* name, int magic) {
  JSCFunctionType ft;
  ft.getter_magic = fn;
  return JS_NewCFunction2(ctx, ft.generic, name, 0, JS_CFUNC_getter_magic,
                          magic);
}

JSValue MakeSetterMagic(
    JSContext* ctx,
    JSValue (*fn)(JSContext*, JSValueConst, JSValueConst, int),
    const char* name, int magic) {
  JSCFunctionType ft;
  ft.setter_magic = fn;
  return JS_NewCFunction2(ctx, ft.generic, name, 1, JS_CFUNC_setter_magic,
                          magic);
}

// ----- Element wrapper creation -----

JSValue WrapElement(JSContext* ctx, dom::Element* el) {
  if (!el) return JS_NULL;

  JSValue obj = JS_NewObjectClass(ctx, static_cast<int>(s_element_class_id));
  if (JS_IsException(obj)) return obj;
  JS_SetOpaque(obj, el);
  return obj;
}

// ----- DOM tree traversal -----

dom::Element* FindElementById(dom::Element* root, StringView id) {
  if (root->id().view() == id) return root;
  for (dom::Node* child = root->first_child(); child;
       child = child->next_sibling()) {
    if (child->is_element()) {
      auto* el = static_cast<dom::Element*>(child);
      dom::Element* found = FindElementById(el, id);
      if (found) return found;
    }
  }
  return nullptr;
}

// ----- document global -----

JSValue DocGetElementById(JSContext* ctx, JSValueConst /*this_val*/, int argc,
                          JSValueConst* argv) {
  auto* data = GetData(ctx);
  if (argc < 1 || !data || !data->doc) return JS_NULL;

  const char* id_str = JS_ToCString(ctx, argv[0]);
  if (!id_str) return JS_NULL;

  dom::Element* el = FindElementById(data->doc, StringView(id_str));
  JS_FreeCString(ctx, id_str);

  if (!el) return JS_NULL;
  return WrapElement(ctx, el);
}

// ----- Registration -----

void RegisterStyleClass(JSContext* ctx) {
  JSRuntime* rt = JS_GetRuntime(ctx);
  if (s_style_class_id == 0) {
    JS_NewClassID(rt, &s_style_class_id);
  }
  if (!JS_IsRegisteredClass(rt, s_style_class_id)) {
    JS_NewClass(rt, s_style_class_id, &g_style_class_def);
  }

  JSValue proto = JS_NewObject(ctx);

  for (int i = 0; i < kStyleMappingCount; ++i) {
    JSAtom atom = JS_NewAtom(ctx, kStyleMappings[i].camel);
    JS_DefinePropertyGetSet(
        ctx, proto, atom,
        MakeGetterMagic(ctx, StyleGetProp, "get", i),
        MakeSetterMagic(ctx, StyleSetProp, "set", i), 0);
    JS_FreeAtom(ctx, atom);
  }

  JS_SetClassProto(ctx, s_style_class_id, proto);
}

void RegisterElementClass(JSContext* ctx) {
  JSRuntime* rt = JS_GetRuntime(ctx);
  if (s_element_class_id == 0) {
    JS_NewClassID(rt, &s_element_class_id);
  }
  if (!JS_IsRegisteredClass(rt, s_element_class_id)) {
    JS_NewClass(rt, s_element_class_id, &g_element_class_def);
  }

  JSValue proto = JS_NewObject(ctx);

  JS_SetPropertyStr(
      ctx, proto, "getAttribute",
      JS_NewCFunction(ctx, ElementGetAttribute, "getAttribute", 1));
  JS_SetPropertyStr(
      ctx, proto, "setAttribute",
      JS_NewCFunction(ctx, ElementSetAttribute, "setAttribute", 2));
  JS_SetPropertyStr(
      ctx, proto, "addEventListener",
      JS_NewCFunction(ctx, ElementAddEventListener, "addEventListener", 2));
  JS_SetPropertyStr(
      ctx, proto, "removeEventListener",
      JS_NewCFunction(ctx, ElementRemoveEventListener, "removeEventListener",
                      1));

  JSAtom tag_atom = JS_NewAtom(ctx, "tagName");
  JS_DefinePropertyGetSet(
      ctx, proto, tag_atom,
      MakeGetter(ctx, ElementGetTagName, "get tagName"),
      JS_UNDEFINED, 0);
  JS_FreeAtom(ctx, tag_atom);

  JSAtom id_atom = JS_NewAtom(ctx, "id");
  JS_DefinePropertyGetSet(
      ctx, proto, id_atom,
      MakeGetter(ctx, ElementGetId, "get id"),
      JS_UNDEFINED, 0);
  JS_FreeAtom(ctx, id_atom);

  JSAtom tc_atom = JS_NewAtom(ctx, "textContent");
  JS_DefinePropertyGetSet(
      ctx, proto, tc_atom,
      MakeGetter(ctx, ElementGetTextContent, "get textContent"),
      MakeSetter(ctx, ElementSetTextContent, "set textContent"), 0);
  JS_FreeAtom(ctx, tc_atom);

  JSAtom style_atom = JS_NewAtom(ctx, "style");
  JS_DefinePropertyGetSet(
      ctx, proto, style_atom,
      MakeGetter(ctx, ElementGetStyle, "get style"),
      JS_UNDEFINED, 0);
  JS_FreeAtom(ctx, style_atom);

  JS_SetClassProto(ctx, s_element_class_id, proto);
}

void RegisterDocumentObject(JSContext* ctx, dom::Document* /*doc*/) {
  // Document pointer now lives in DomBindings::InstanceData and is
  // retrieved per-call via JS_GetContextOpaque. Here we just install the
  // JS-visible `document` global with its methods.
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue doc_obj = JS_NewObject(ctx);

  JS_SetPropertyStr(
      ctx, doc_obj, "getElementById",
      JS_NewCFunction(ctx, DocGetElementById, "getElementById", 1));

  JS_SetPropertyStr(ctx, global, "document", doc_obj);
  JS_FreeValue(ctx, global);
}

}  // namespace

DomBindings::DomBindings() : data_(std::make_unique<InstanceData>()) {}
DomBindings::~DomBindings() { Unbind(); }

bool DomBindings::bound() const { return data_->ctx != nullptr; }
event::EventManager* DomBindings::event_manager() const { return data_->em; }
dom::Document* DomBindings::document() const { return data_->doc; }
JSContext* DomBindings::context() const { return data_->ctx; }

void DomBindings::SetDevtoolTargetDocument(dom::Document* target) {
  data_->devtool_target_doc = target;
}
dom::Document* DomBindings::devtool_target_document() const {
  return data_->devtool_target_doc;
}
void DomBindings::SetPerfOverlay(vx::devtool::overlay::PerfOverlay* perf) {
  data_->perf_overlay = perf;
}
vx::devtool::overlay::PerfOverlay* DomBindings::perf_overlay() const {
  return data_->perf_overlay;
}
void DomBindings::SetHotReloadManager(
    vx::devtool::hot_reload::HotReloadManager* mgr) {
  data_->hot_reload_manager = mgr;
}
vx::devtool::hot_reload::HotReloadManager*
DomBindings::hot_reload_manager() const {
  return data_->hot_reload_manager;
}
void DomBindings::SetRedactionPolicy(dom::RedactionPolicy policy) {
  data_->redaction_policy = policy;
}
dom::RedactionPolicy DomBindings::redaction_policy() const {
  return data_->redaction_policy;
}

void DomBindings::Bind(JSContext* ctx, dom::Document* doc,
                       event::EventManager* em) {
  Unbind();
  data_->ctx = ctx;
  data_->doc = doc;
  data_->em = em;

  // B6 (TASK-20260419-01): if EventManager is destroyed before us, drop our
  // pointer to it so Unbind/AddEventListener become safe no-ops. This relaxes
  // the previous strict "DomBindings must be destroyed first" host contract.
  if (em != nullptr) {
    InstanceData* data = data_.get();
    data_->em_observer_token = em->AddDestructionObserver([data]() {
      data->em = nullptr;
      data->listener_elements.clear();
      data->em_observer_token = 0;
    });
  }

  JS_SetContextOpaque(ctx, this);
  data_->tracked_callbacks.ctx = ctx;

  RegisterStyleClass(ctx);
  RegisterElementClass(ctx);
  RegisterDocumentObject(ctx, doc);
}

void DomBindings::Unbind() {
  // #50: Tear down EventManager lambdas BEFORE freeing the JS callbacks
  // those lambdas captured. Freeing first would leave em_ holding
  // lambdas that reference already-freed JSValue callbacks, and any
  // later dispatch (e.g. HandleInput) would be use-after-free.
  //
  // B6 (TASK-20260419-01): destruction order is now permissive. The two
  // valid orderings are handled here:
  //   - DomBindings first: em_ != nullptr; deregister our observer (so it
  //     can never fire on the freed InstanceData) and tear down listeners.
  //   - EventManager first: the observer already fired and set em_ = nullptr
  //     and cleared listener_elements; this branch is a no-op.
  if (data_->em) {
    if (data_->em_observer_token != 0) {
      data_->em->RemoveDestructionObserver(data_->em_observer_token);
      data_->em_observer_token = 0;
    }
    for (usize i = 0; i < data_->listener_elements.size(); ++i) {
      data_->em->RemoveEventListeners(data_->listener_elements[i]);
    }
  }
  data_->listener_elements.clear();
  data_->tracked_callbacks.FreeAll();
  if (data_->ctx) {
    JS_SetContextOpaque(data_->ctx, nullptr);
    JSValue global = JS_GetGlobalObject(data_->ctx);
    JSAtom doc_atom = JS_NewAtom(data_->ctx, "document");
    JS_DeleteProperty(data_->ctx, global, doc_atom, 0);
    JS_FreeAtom(data_->ctx, doc_atom);
    JS_FreeValue(data_->ctx, global);
  }
  data_->ctx = nullptr;
  data_->doc = nullptr;
  data_->em = nullptr;
  data_->devtool_target_doc = nullptr;
  data_->perf_overlay = nullptr;
  data_->hot_reload_manager = nullptr;
}

// =============================================================================
// TASK-20260502-01 A.1.4 — DevTool dogfood UI native binding
//
// `RegisterDevtoolBindings(ctx)` installs the global JS function
// `vx_devtool_get_dom_json()` on `ctx`'s global object. The function
// recovers the target Document via JS_GetContextOpaque(ctx) →
// DomBindings → devtool_target_document() and returns
// `vx::devtool::SerializeDocument(target, kRedactSensitive)` as a JS
// string.
//
// When VX_BUILD_DEVTOOL=OFF the implementation is a no-op stub (A14
// zero-byte guard). The vx_devtool dependency (which provides
// SerializeDocument) is only linked into vx_script when DEVTOOL=ON; see
// veloxa/script/CMakeLists.txt.
// =============================================================================

#ifdef VX_BUILD_DEVTOOL

}  // namespace vx::script

#include <cstdio>

#include "veloxa/devtool/hot_reload/hot_reload_manager.h"
#include "veloxa/devtool/inspector/inspector_data.h"
#include "veloxa/devtool/overlay/perf_overlay.h"

namespace vx::script {

namespace {

JSValue VxDevtoolGetDomJson(JSContext* ctx, JSValueConst /*this_val*/,
                            int /*argc*/, JSValueConst* /*argv*/) {
  auto* bindings = static_cast<DomBindings*>(JS_GetContextOpaque(ctx));
  dom::Document* target =
      bindings ? bindings->devtool_target_document() : nullptr;
  if (target == nullptr) {
    static constexpr char kNullJson[] = "null";
    return JS_NewStringLen(ctx, kNullJson, sizeof(kNullJson) - 1);
  }
  /* A.2.1: pull policy from DomBindings (synced via Application by
   * vx_inspector_set_redaction_policy). Defaults to kRedactSensitive. */
  vx::String json = vx::devtool::SerializeDocument(
      target, bindings->redaction_policy());
  return JS_NewStringLen(ctx, json.data(), json.size());
}

// TASK-20260502-02 B.2.2 — vx_view_get_perf_stats() native binding.
// Returns a JSON envelope:
//   {"fps":N,"style":S,"layout":L,"render":R,"paint":P,"total":T,
//    "frame_count":F,"abort_count":A}
// All numbers are integers (rounded from PerfOverlay sliding average) so
// the JS HUD can format them cheaply with no decimal noise.
// When PerfOverlay is unattached returns the zero-stats envelope.
JSValue VxViewGetPerfStats(JSContext* ctx, JSValueConst /*this_val*/,
                           int /*argc*/, JSValueConst* /*argv*/) {
  auto* bindings = static_cast<DomBindings*>(JS_GetContextOpaque(ctx));
  vx::devtool::overlay::PerfOverlay* perf =
      bindings ? bindings->perf_overlay() : nullptr;

  // Format buffer is bounded; max ~150 chars even with abort_count=u64-max.
  char buf[256];
  if (perf == nullptr) {
    std::snprintf(buf, sizeof(buf),
                  "{\"fps\":0,\"style\":0,\"layout\":0,\"render\":0,"
                  "\"paint\":0,\"total\":0,\"frame_count\":0,"
                  "\"abort_count\":0}");
  } else {
    auto agg = perf->aggregated();
    const int fps        = static_cast<int>(perf->fps() + 0.5f);
    const int style_ms   = static_cast<int>(agg.style_ms + 0.5f);
    const int layout_ms  = static_cast<int>(agg.layout_ms + 0.5f);
    const int render_ms  = static_cast<int>(agg.render_ms + 0.5f);
    const int paint_ms   = static_cast<int>(agg.paint_ms + 0.5f);
    const int total_ms   = static_cast<int>(agg.total_ms + 0.5f);
    std::snprintf(buf, sizeof(buf),
                  "{\"fps\":%d,\"style\":%d,\"layout\":%d,\"render\":%d,"
                  "\"paint\":%d,\"total\":%d,\"frame_count\":%llu,"
                  "\"abort_count\":%llu}",
                  fps, style_ms, layout_ms, render_ms, paint_ms, total_ms,
                  static_cast<unsigned long long>(perf->frame_count()),
                  static_cast<unsigned long long>(perf->abort_count()));
  }
  return JS_NewString(ctx, buf);
}

// TASK-20260503-01 C.3.1 — vx_devtool_get_hot_reload_status() native binding.
// Returns a JSON envelope:
//   {"tracked":N,"last_error":"..."}
// The HUD JS uses this to render a watching/error indicator (see
// inspector_panel.js updateHotReloadStatus). When no HotReloadManager
// is attached returns the safe zero envelope so the UI parse never throws.
//
// last_error is JSON-escaped with the minimal set of CSS path characters
// likely to appear: backslash, double-quote, and control bytes < 0x20.
// HotReloadManager only ever sets last_error to a fixed prefix + filesystem
// path (see HotReloadManager::DrainEvents), so we don't need full UTF-8
// escape coverage — but we still defensively encode anything <0x20 as \u00XX.
JSValue VxDevtoolGetHotReloadStatus(JSContext* ctx, JSValueConst /*this_val*/,
                                    int /*argc*/, JSValueConst* /*argv*/) {
  auto* bindings = static_cast<DomBindings*>(JS_GetContextOpaque(ctx));
  vx::devtool::hot_reload::HotReloadManager* mgr =
      bindings ? bindings->hot_reload_manager() : nullptr;

  unsigned long long tracked = 0;
  std::string err;
  if (mgr != nullptr) {
    tracked = static_cast<unsigned long long>(mgr->tracked_count());
    err = mgr->last_error();
  }

  std::string escaped;
  escaped.reserve(err.size() + 8);
  for (unsigned char c : err) {
    if (c == '"' || c == '\\') {
      escaped.push_back('\\');
      escaped.push_back(static_cast<char>(c));
    } else if (c < 0x20) {
      char hex[8];
      std::snprintf(hex, sizeof(hex), "\\u%04x", c);
      escaped.append(hex);
    } else {
      escaped.push_back(static_cast<char>(c));
    }
  }

  std::string out;
  out.reserve(escaped.size() + 48);
  out.append("{\"tracked\":");
  char num_buf[32];
  std::snprintf(num_buf, sizeof(num_buf), "%llu", tracked);
  out.append(num_buf);
  out.append(",\"last_error\":\"");
  out.append(escaped);
  out.append("\"}");

  return JS_NewStringLen(ctx, out.data(), out.size());
}

}  // namespace

void RegisterDevtoolBindings(JSContext* ctx) {
  JSValue global = JS_GetGlobalObject(ctx);
  JS_SetPropertyStr(
      ctx, global, "vx_devtool_get_dom_json",
      JS_NewCFunction(ctx, VxDevtoolGetDomJson,
                      "vx_devtool_get_dom_json", 0));
  // B.2.2 — Performance Overlay live stats binding.
  JS_SetPropertyStr(
      ctx, global, "vx_view_get_perf_stats",
      JS_NewCFunction(ctx, VxViewGetPerfStats,
                      "vx_view_get_perf_stats", 0));
  // C.3.1 — Hot Reload status binding (DevTool UI status indicator).
  JS_SetPropertyStr(
      ctx, global, "vx_devtool_get_hot_reload_status",
      JS_NewCFunction(ctx, VxDevtoolGetHotReloadStatus,
                      "vx_devtool_get_hot_reload_status", 0));
  JS_FreeValue(ctx, global);
}

#else  // VX_BUILD_DEVTOOL OFF — A14 zero-byte stub guard

void RegisterDevtoolBindings(JSContext* /*ctx*/) {
  // Intentionally empty: dogfood UI is not compiled when DEVTOOL=OFF.
}

#endif  // VX_BUILD_DEVTOOL

}  // namespace vx::script
