// TASK-20260503-04 D.3: DevTool Console panel JS.
//
// Embedded inline (__INLINE_CONSOLE_JS__ replacement) inside
// inspector_panel.html <script> after the existing inspector_panel.js
// content. Implements two contracts on the Console scope, both keyed
// off the existing tab-switch protocol (data-tab="console").
//
// 1. REPL input — Veloxa's <input> element has no native keyboard /
//    value-attribute behaviour, so we maintain `inputValue` ourselves
//    and listen on window for keydown. The handler is gated by an
//    isConsoleActive() check so typing on the DOM/Style/Layout tabs
//    never enters the REPL. Enter calls vx_console_eval (D.4 will
//    register this); Backspace deletes the last char; printable
//    single-character keys append. Other keys (F-keys, arrows,
//    modifier combos) fall through to host hotkeys.
//
// 2. console.log drain — vx_devtool_get_console_log_drain (D.2) is
//    polled every 200 ms (5 fps) and any returned messages are
//    appended to #console-output with a level-coloured class. The
//    `dropped_count` counter is surfaced as a dim "[dropped N]" line
//    so the developer notices T6 mitigation kicked in.
//
// Defensive: every binding is feature-tested (typeof === "function")
// because the inspector_panel script also runs without these endpoints
// in the legacy boot path; we never throw on a missing binding.

(function consoleRepl() {
  var inputValue = "";

  function el(id) { return document.getElementById(id); }

  function isConsoleActive() {
    var btn = document.querySelector(
        "#devtool-tabs button[data-tab='console']");
    if (!btn || !btn.className) return false;
    return btn.className.indexOf("active") >= 0;
  }

  function appendLine(level, text) {
    var output = el("console-output");
    if (!output) return;
    var div = document.createElement("div");
    div.className = "console-line console-" + level;
    if (typeof div.textContent !== "undefined") {
      div.textContent = text;
    } else {
      // Fallback for engines without textContent setter.
      div.innerHTML = String(text);
    }
    if (typeof output.appendChild === "function") {
      output.appendChild(div);
    }
  }

  function refreshInputDisplay() {
    var display = el("console-input-display");
    if (!display) return;
    if (typeof display.textContent !== "undefined") {
      display.textContent = inputValue;
    } else {
      display.innerHTML = String(inputValue);
    }
  }

  function evalCurrent() {
    var src = inputValue;
    if (src.length === 0) return;
    appendLine("input", "JS> " + src);
    inputValue = "";
    refreshInputDisplay();
    if (typeof vx_console_eval !== "function") {
      appendLine("meta", "[vx_console_eval not bound — D.4 pending]");
      return;
    }
    var result = vx_console_eval(src);
    // Result envelope: {"status":N,"value":"...","interrupted":<bool>}
    // (D.4 spec). Defensive parse so a missing field prints raw.
    try {
      var env = JSON.parse(result);
      if (env && typeof env.status === "number") {
        if (env.status === 0) {
          appendLine("result", "= " + (env.value || "undefined"));
        } else if (env.interrupted) {
          appendLine("error",
              "INTERRUPTED (script aborted by interrupt budget)");
        } else {
          appendLine("error",
              "ERROR(" + env.status + "): " + (env.value || ""));
        }
        return;
      }
    } catch (e) { /* fall through to raw */ }
    appendLine("result", "= " + result);
  }

  function onKeydown(e) {
    if (!isConsoleActive()) return;
    if (!e || typeof e.key !== "string") return;
    if (e.key === "Enter") {
      evalCurrent();
      if (typeof e.preventDefault === "function") e.preventDefault();
      return;
    }
    if (e.key === "Backspace") {
      if (inputValue.length > 0) {
        inputValue = inputValue.substring(0, inputValue.length - 1);
        refreshInputDisplay();
      }
      if (typeof e.preventDefault === "function") e.preventDefault();
      return;
    }
    if (e.key.length === 1) {
      // Printable single-char key — letters / digits / punctuation / space.
      inputValue += e.key;
      refreshInputDisplay();
      if (typeof e.preventDefault === "function") e.preventDefault();
    }
    // Other keys (F11/F12/arrows/Ctrl combos) — let host handle.
  }

  function drainConsoleLog() {
    if (typeof vx_devtool_get_console_log_drain !== "function") return;
    var raw = vx_devtool_get_console_log_drain();
    var env;
    try { env = JSON.parse(raw); } catch (_) { return; }
    if (!env || !env.messages) return;
    for (var i = 0; i < env.messages.length; i++) {
      var m = env.messages[i];
      appendLine(m.level || "log", m.text || "");
    }
    if (env.dropped_count && env.dropped_count > 0) {
      appendLine("meta",
          "[dropped " + env.dropped_count + " older messages]");
    }
    if (env.truncated) {
      appendLine("meta",
          "[some messages exceeded 4 KiB and were truncated]");
    }
  }

  // Wire up — defensive because the inspector_panel.js prologue runs
  // before this IIFE in some hot-reload scenarios.
  if (typeof window !== "undefined" &&
      typeof window.addEventListener === "function") {
    window.addEventListener("keydown", onKeydown);
  }
  if (typeof setInterval === "function") {
    setInterval(drainConsoleLog, 200);  // 5 fps poll — see creative §C4
  }
})();
