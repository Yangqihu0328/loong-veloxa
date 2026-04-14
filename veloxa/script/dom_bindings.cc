#include "veloxa/script/dom_bindings.h"

extern "C" {
#include "quickjs.h"
}

#include <cstring>

#include "veloxa/core/css/parser.h"
#include "veloxa/core/css/property.h"
#include "veloxa/core/dom/text.h"
#include "veloxa/foundation/strings/interned_string.h"
#include "veloxa/foundation/strings/string.h"

namespace vx::script {

namespace {

DomBindings* GetBindings(JSContext* ctx) {
  return static_cast<DomBindings*>(JS_GetContextOpaque(ctx));
}

event::EventManager* GetEventManager(JSContext* ctx) {
  auto* b = GetBindings(ctx);
  return b ? b->event_manager() : nullptr;
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

JSClassID g_element_class_id = 0;

dom::Element* GetElement(JSContext* /*ctx*/, JSValueConst this_val) {
  return static_cast<dom::Element*>(
      JS_GetOpaque(this_val, g_element_class_id));
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

JSClassID g_style_class_id = 0;

struct StyleOpaque {
  dom::Element* element;
};

StyleOpaque* GetStyleOpaque(JSValueConst this_val) {
  return static_cast<StyleOpaque*>(
      JS_GetOpaque(this_val, g_style_class_id));
}

void StyleFinalizer(JSRuntime* /*rt*/, JSValueConst val) {
  auto* so = static_cast<StyleOpaque*>(
      JS_GetOpaque(val, g_style_class_id));
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

// ----- Style property magic getter/setter -----

JSValue StyleGetProp(JSContext* ctx, JSValueConst this_val, int magic) {
  auto* so = GetStyleOpaque(this_val);
  if (!so || !so->element || magic < 0 || magic >= kStyleMappingCount) {
    return JS_NewString(ctx, "");
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

// ----- Tracked JS callbacks for cleanup -----

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
};

TrackedCallbacks g_tracked_callbacks;

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
  auto* em = GetEventManager(ctx);
  if (!em) {
    JS_FreeValue(ctx, callback);
    return JS_UNDEFINED;
  }

  g_tracked_callbacks.Track(callback);

  JSContext* captured_ctx = ctx;
  em->AddEventListener(
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

  return JS_UNDEFINED;
}

JSValue ElementRemoveEventListener(JSContext* ctx, JSValueConst this_val,
                                   int /*argc*/, JSValueConst* /*argv*/) {
  auto* el = GetElement(ctx, this_val);
  auto* em = GetEventManager(ctx);
  if (!el || !em) return JS_UNDEFINED;
  em->RemoveEventListeners(el);
  return JS_UNDEFINED;
}

JSValue ElementGetStyle(JSContext* ctx, JSValueConst this_val) {
  auto* el = GetElement(ctx, this_val);
  if (!el) return JS_UNDEFINED;

  JSValue style_obj =
      JS_NewObjectClass(ctx, static_cast<int>(g_style_class_id));
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

  JSValue obj = JS_NewObjectClass(ctx, static_cast<int>(g_element_class_id));
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

dom::Document* g_bound_doc = nullptr;

JSValue DocGetElementById(JSContext* ctx, JSValueConst /*this_val*/, int argc,
                          JSValueConst* argv) {
  if (argc < 1 || !g_bound_doc) return JS_NULL;

  const char* id_str = JS_ToCString(ctx, argv[0]);
  if (!id_str) return JS_NULL;

  dom::Element* el = FindElementById(g_bound_doc, StringView(id_str));
  JS_FreeCString(ctx, id_str);

  if (!el) return JS_NULL;
  return WrapElement(ctx, el);
}

// ----- Registration -----

void RegisterStyleClass(JSContext* ctx) {
  JSRuntime* rt = JS_GetRuntime(ctx);
  JS_NewClassID(rt, &g_style_class_id);
  JS_NewClass(rt, g_style_class_id, &g_style_class_def);

  JSValue proto = JS_NewObject(ctx);

  for (int i = 0; i < kStyleMappingCount; ++i) {
    JSAtom atom = JS_NewAtom(ctx, kStyleMappings[i].camel);
    JS_DefinePropertyGetSet(
        ctx, proto, atom,
        MakeGetterMagic(ctx, StyleGetProp, "get", i),
        MakeSetterMagic(ctx, StyleSetProp, "set", i), 0);
    JS_FreeAtom(ctx, atom);
  }

  JS_SetClassProto(ctx, g_style_class_id, proto);
}

void RegisterElementClass(JSContext* ctx) {
  JSRuntime* rt = JS_GetRuntime(ctx);
  JS_NewClassID(rt, &g_element_class_id);
  JS_NewClass(rt, g_element_class_id, &g_element_class_def);

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

  JS_SetClassProto(ctx, g_element_class_id, proto);
}

void RegisterDocumentObject(JSContext* ctx, dom::Document* doc) {
  g_bound_doc = doc;

  JSValue global = JS_GetGlobalObject(ctx);
  JSValue doc_obj = JS_NewObject(ctx);

  JS_SetPropertyStr(
      ctx, doc_obj, "getElementById",
      JS_NewCFunction(ctx, DocGetElementById, "getElementById", 1));

  JS_SetPropertyStr(ctx, global, "document", doc_obj);
  JS_FreeValue(ctx, global);
}

}  // namespace

DomBindings::~DomBindings() { Unbind(); }

void DomBindings::Bind(JSContext* ctx, dom::Document* doc,
                       event::EventManager* em) {
  Unbind();
  ctx_ = ctx;
  doc_ = doc;
  em_ = em;

  JS_SetContextOpaque(ctx, this);

  g_tracked_callbacks.ctx = ctx;

  RegisterStyleClass(ctx);
  RegisterElementClass(ctx);
  RegisterDocumentObject(ctx, doc);
}

void DomBindings::Unbind() {
  g_tracked_callbacks.FreeAll();
  if (ctx_) {
    JS_SetContextOpaque(ctx_, nullptr);
    JSValue global = JS_GetGlobalObject(ctx_);
    JSAtom doc_atom = JS_NewAtom(ctx_, "document");
    JS_DeleteProperty(ctx_, global, doc_atom, 0);
    JS_FreeAtom(ctx_, doc_atom);
    JS_FreeValue(ctx_, global);
  }
  g_bound_doc = nullptr;
  ctx_ = nullptr;
  doc_ = nullptr;
  em_ = nullptr;
}

}  // namespace vx::script
