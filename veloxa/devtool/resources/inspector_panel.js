// TASK-20260502-01 A.1.3: DevTool Inspector UI dogfood JS.
//
// Loaded inline inside inspector_panel.html (replaces __INLINE_JS__
// placeholder at runtime by Application::LoadDevtoolDocument, A.1.6).
//
// Native binding contract (A.1.4 will land):
//   vx_devtool_get_dom_json() -> string  // JSON tree of target Document
//   (T3 redaction policy applied; see veloxa/devtool/inspector/
//    inspector_data.h SerializeDocument).
//
// Tab switch contract: clicking a button toggles the .active class on
// itself (and removes it from siblings) and on the matching panel div
// (id = ${data-tab}-tree-panel for DOM, ${data-tab}-panel otherwise).

function escapeHtml(s) {
  return String(s)
      .replace(/&/g, "&amp;")
      .replace(/</g, "&lt;")
      .replace(/>/g, "&gt;");
}

function renderTreeNode(node) {
  if (node === null || node === undefined) return "";
  if (node.text !== undefined) {
    return '<div class="text">' + escapeHtml(node.text) + "</div>";
  }
  var html = '<div class="elem">&lt;' + escapeHtml(node.tag) + "&gt;";
  if (node.children) {
    for (var i = 0; i < node.children.length; i++) {
      html += renderTreeNode(node.children[i]);
    }
  }
  html += "&lt;/" + escapeHtml(node.tag) + "&gt;</div>";
  return html;
}

function renderDomTree() {
  var raw = vx_devtool_get_dom_json();
  var root = JSON.parse(raw);
  // Envelope: { "document": { "type": "document", "children": [...] } }
  var doc = root && root.document ? root.document : root;
  var panel = document.getElementById("dom-tree-panel");
  if (!panel) return;
  if (doc && doc.children) {
    var html = "";
    for (var i = 0; i < doc.children.length; i++) {
      html += renderTreeNode(doc.children[i]);
    }
    panel.innerHTML = html;
  } else {
    panel.innerHTML = "<div>(empty)</div>";
  }
}

function setupTabs() {
  var tabs = document.getElementById("devtool-tabs");
  if (!tabs) return;
  var buttons = tabs.children;
  for (var i = 0; i < buttons.length; i++) {
    (function(btn) {
      btn.addEventListener("click", function() {
        var which = btn.getAttribute("data-tab");
        // Toggle button active state
        for (var j = 0; j < buttons.length; j++) {
          buttons[j].className = (buttons[j] === btn) ? "active" : "";
        }
        // Toggle panel active state
        var panels = ["dom-tree-panel", "style-panel", "layout-panel"];
        var targets = ["dom", "style", "layout"];
        for (var k = 0; k < panels.length; k++) {
          var p = document.getElementById(panels[k]);
          if (p) p.className = (targets[k] === which) ? "active" : "";
        }
      });
    })(buttons[i]);
  }
}

setupTabs();
renderDomTree();
