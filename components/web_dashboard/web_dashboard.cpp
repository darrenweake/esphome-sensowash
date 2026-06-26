#include "web_dashboard.h"
#ifdef USE_ESP32
#include "esphome/core/log.h"
#include "dashboard_fonts.h"  // FONTS_CSS: DuraSans (Duravit brand font) woff2, base64-inlined
#include "dashboard_bg.h"     // BG_LAND / BG_PORT: Duravit product photo (landscape + portrait)

namespace esphome {
namespace web_dashboard {

static const char *const TAG = "web_dashboard";

// Tablet-styled single-page dashboard. Pure HTML/CSS/JS, no external assets.
// Talks to the ESPHome web_server REST API (POST /switch|button|select/...) and
// the /events SSE stream for live state. Entities are matched by their friendly
// name (sent in the SSE config burst on connect), so object-id sanitisation
// quirks don't matter.
static const char DASHBOARD_HTML[] = R"DASH(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<title>Duravit SensoWash Bridge</title>
<link rel="stylesheet" href="/fonts.css">
<link rel="icon" type="image/png" href="data:image/png;base64,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">
<style>
  /* SensoWash app palette (extracted from the APK): brand blue #034884 + near-white #fcfdff. */
  :root{
    --bg1:#034884; --bg2:#02284c; --card:rgba(4,46,86,.66);
    --card-br:rgba(255,255,255,.16); --txt:#fcfdff; --muted:#bcd0e6;
    --amber:#5aa9e8; --amber-d:#0a3d6b; --glass:rgba(255,255,255,.10); --glass-br:rgba(255,255,255,.24);
    --red:#e0584e; --green:#79e387; --blue:#3a93e6;
  }
  *{box-sizing:border-box}
  /* Every button shares the brand font + base sizing (Material-style, consistent across the UI). */
  button{font-family:inherit;font-size:14px;letter-spacing:.2px}
  body{margin:0;font-family:'DuraSans',-apple-system,Segoe UI,Roboto,Helvetica,Arial,sans-serif;
    color:var(--txt);
    background:linear-gradient(160deg,rgba(2,32,60,.5),rgba(1,12,28,.72)),url(/bg.jpg) center/cover fixed;
    min-height:100vh;padding:0 0 64px}
  /* Mobile/portrait gets the portrait-cropped Duravit photo; desktop keeps the landscape one. */
  @media (orientation:portrait){
    body{background:linear-gradient(160deg,rgba(2,32,60,.5),rgba(1,12,28,.72)),url(/bgp.jpg) center/cover fixed}
  }
  header{display:flex;align-items:baseline;justify-content:space-between;gap:12px;
    flex-wrap:wrap;padding:16px 20px 10px;border-bottom:1px solid var(--card-br)}
  header h1{font-size:18px;margin:0;font-weight:600;letter-spacing:.3px}
  header h1 b{color:var(--amber)}
  .titlewrap{display:flex;flex-direction:column;gap:3px}
  #clock{font-size:13px;color:var(--muted);font-variant-numeric:tabular-nums;letter-spacing:.3px}
  /* Pairing/enrolment banner (only shows when a new toilet needs its Bluetooth button pressed) */
  #pairbanner{display:none;max-width:1100px;margin:14px auto 0;padding:13px 18px;border-radius:14px;
    background:rgba(58,147,230,.22);border:1px solid var(--blue);color:var(--txt);font-size:15px;
    text-align:center;backdrop-filter:blur(10px);-webkit-backdrop-filter:blur(10px)}
  #pairbanner.show{display:block}
  #pairbanner.ok{background:rgba(121,227,135,.25);border-color:var(--green)}
  #devinfo{font-size:12px;color:var(--muted);font-variant-numeric:tabular-nums}
  .grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));
    gap:14px;padding:16px 20px;max-width:1100px;margin:0 auto}
  .card{background:var(--card);border:1px solid var(--card-br);border-radius:16px;
    padding:14px 16px;backdrop-filter:blur(14px) saturate(1.2);-webkit-backdrop-filter:blur(14px) saturate(1.2);
    box-shadow:0 1px 2px rgba(0,0,0,.35),0 2px 6px rgba(0,0,0,.18),inset 0 1px 0 rgba(255,255,255,.06)}
  .card h2{margin:0 0 12px;font-size:13px;text-transform:uppercase;letter-spacing:1px;
    color:var(--muted);font-weight:600;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
  .row{display:flex;align-items:center;justify-content:space-between;gap:12px;
    padding:7px 0}
  .row .lbl{font-size:14px}
  .seg{display:flex;gap:6px;flex-wrap:wrap;margin:6px 0 12px}
  .seg button{flex:1 1 0;min-width:0;white-space:nowrap;padding:10px 6px;border:1px solid var(--glass-br);
    background:var(--glass);color:var(--txt);border-radius:9px;font-size:14px;cursor:pointer;
    transition:.12s}
  .seg button:active{transform:scale(.97)}
  .seg button.sel{background:var(--amber);border-color:var(--amber);color:#04243f;font-weight:600}
  .seg .lbl{width:100%;font-size:12px;color:var(--muted);margin-bottom:2px}
  .toggle{width:48px;height:28px;border-radius:16px;background:var(--glass);
    border:1px solid var(--glass-br);position:relative;cursor:pointer;transition:.15s;flex:0 0 auto}
  .toggle::after{content:"";position:absolute;top:2px;left:2px;width:22px;height:22px;
    border-radius:50%;background:#cfd3d7;transition:.15s}
  .toggle.on{background:var(--amber);border-color:var(--amber)}
  .toggle.on::after{left:24px;background:#fff}
  .act{display:block;width:100%;padding:12px;margin:6px 0;border-radius:10px;cursor:pointer;
    border:1px solid var(--glass-br);background:var(--glass);color:var(--txt);font-size:14px;
    text-align:left;display:flex;justify-content:space-between;align-items:center}
  .act:active{transform:scale(.99)}
  .act .go{font-size:14px;color:var(--amber);font-weight:600}
  .act.danger{border-color:#7d2c22}.act.danger .go{color:#ff7a6b}
  .big{font-size:16px;font-weight:600;padding:16px}
  #status{position:fixed;left:0;right:0;bottom:0;display:flex;gap:18px;flex-wrap:wrap;
    align-items:center;padding:10px 20px;background:rgba(2,24,46,.5);
    border-top:1px solid var(--card-br);font-size:13px;backdrop-filter:blur(16px);-webkit-backdrop-filter:blur(16px)}
  .pill{display:flex;align-items:center;gap:7px;color:var(--muted)}
  .dot{width:10px;height:10px;border-radius:50%;background:#555;flex:0 0 auto}
  .dot.on{background:var(--green)}.dot.off{background:#666}.dot.warn{background:var(--red)}
  .dot.blue{background:var(--blue)}
  #faults{cursor:pointer}
  #faults.err{color:#ff7a6b;font-weight:600}
  .modal{position:fixed;inset:0;background:rgba(0,0,0,.6);display:none;align-items:center;
    justify-content:center;padding:24px;z-index:10}
  .modal.show{display:flex}
  .sheet{background:rgba(6,40,80,.72);backdrop-filter:blur(20px) saturate(1.3);-webkit-backdrop-filter:blur(20px) saturate(1.3);
    border:1px solid var(--card-br);border-radius:14px;padding:20px;max-width:420px;width:100%}
  .sheet h3{margin:0 0 10px}.sheet p{color:var(--muted);font-size:14px;line-height:1.5;white-space:pre-line}
  .sheet .btns{display:flex;gap:10px;justify-content:flex-end;margin-top:18px}
  .sheet button{padding:10px 16px;border-radius:9px;border:1px solid var(--glass-br);
    background:var(--glass);color:var(--txt);cursor:pointer;font-size:14px}
  .sheet button.go{background:var(--amber);border-color:var(--amber);color:#04243f;font-weight:600}
  .sheet button.danger{background:var(--red);border-color:var(--red);color:#fff}
  #logwrap{max-width:1100px;margin:0 auto 8px;padding:0 20px}
  #logbar{display:flex;justify-content:space-between;align-items:center;font-size:12px;
    text-transform:uppercase;letter-spacing:1px;color:var(--muted);font-weight:600;margin-bottom:6px}
  .logbtns button{background:var(--glass);border:1px solid var(--glass-br);color:var(--txt);
    border-radius:9px;padding:8px 16px;font-size:14px;cursor:pointer;margin-left:8px;font-weight:600}
  .logbtns button:active{transform:scale(.98)}
  #log{background:rgba(3,22,42,.5);backdrop-filter:blur(12px);-webkit-backdrop-filter:blur(12px);
    border:1px solid var(--card-br);border-radius:10px;height:200px;
    overflow-y:auto;padding:8px 10px;font-family:ui-monospace,Consolas,Menlo,monospace;font-size:12px;
    line-height:1.45;white-space:pre-wrap;word-break:break-all}
  #log .l{color:#aeb4ba}
  #log .tx{color:var(--amber)}
  #log .rx{color:#5fd08a}
  #log .w{color:#ffb454}
  #log .e{color:#ff6b5b}
  #log.hidden{display:none}
  .srow{display:flex;align-items:center;justify-content:space-between;gap:10px;padding:7px 0;font-size:14px;
    border-top:1px solid rgba(255,255,255,.05)}
  .srow:first-of-type{border-top:0}
  .srow .k{color:var(--muted)} .srow .v{font-variant-numeric:tabular-nums;text-align:right}
  .srow .v.warnv{color:#ff7a6b;cursor:pointer}
  .sig{display:inline-flex;align-items:center;gap:9px}
  .bars{display:inline-flex;align-items:flex-end;gap:2px;height:15px}
  .bars i{width:3px;background:rgba(255,255,255,.18);border-radius:1px}
  .bars i.b1{height:5px}.bars i.b2{height:8px}.bars i.b3{height:11px}.bars i.b4{height:15px}
  .bars i.on{background:var(--green)} .bars.weak i.on{background:#f0a23a}
  .bticon{color:#5a6068;display:inline-flex} .bticon.on{color:var(--blue)}
  .build{color:var(--muted);font-variant-numeric:tabular-nums}
  .note{font-size:12px;color:var(--muted);line-height:1.55;padding:2px 0 4px}
  .row.spaced{margin-top:8px;padding-top:12px;border-top:1px solid rgba(255,255,255,.07)}
  .row.disabled{opacity:.4;pointer-events:none}
  /* Connected state shows "Disconnect Bluetooth": only the icon goes red to signal the action;
     the border stays default and the Connect-state icon stays blue. */
  .act.discon .ic{color:var(--red)}
  #conn{display:flex;align-items:center;justify-content:space-between;gap:12px;max-width:1100px;
    margin:12px auto 0;padding:10px 16px;background:var(--card);border:1px solid var(--card-br);border-radius:12px}
  #conn-status{display:flex;align-items:center;gap:9px;font-size:14px}
  .cbtn{padding:9px 20px;border-radius:9px;border:1px solid var(--glass-br);background:var(--glass);
    color:var(--txt);font-size:14px;cursor:pointer;font-weight:600}
  .cbtn.discon{background:var(--blue);border-color:var(--blue);color:#fff}
  .cbtn:active{transform:scale(.98)}
  /* Material-style state layer on every button */
  button:hover{filter:brightness(1.08)}
  /* Leading Material icon + label inside action buttons */
  .act .lead{display:flex;align-items:center;gap:11px;min-width:0;overflow:hidden;text-overflow:ellipsis}
  .act .ic{width:21px;height:21px;flex:0 0 auto;fill:currentColor;color:var(--amber)}
  .act.danger .ic{color:#ff7a6b}
  /* Settings-row leading icon (inherits the row label colour) */
  .wico{display:inline-flex;align-items:center;gap:10px;min-width:0}
  .wico .ic{width:18px;height:18px;flex:0 0 auto;fill:currentColor}
  /* Energy-saving day calendar: a full-day timeline of seat-heating-on (warm) windows */
  .daycal{padding:4px 0 2px}
  .daycal .leg{display:flex;gap:16px;font-size:11px;color:var(--muted);margin-bottom:8px;flex-wrap:wrap}
  .daycal .leg span{display:inline-flex;align-items:center;gap:6px}
  .daycal .leg i{width:12px;height:12px;border-radius:3px;display:inline-block}
  .daycal .leg i.warm{background:linear-gradient(180deg,#f7a93a,#e8820a)}
  .daycal .leg i.eco{background:var(--glass);border:1px solid var(--glass-br)}
  .dtrack{position:relative;height:30px;background:var(--glass);border:1px solid var(--glass-br);
    border-radius:8px;overflow:hidden}
  .dtrack .warm{position:absolute;top:0;bottom:0;background:linear-gradient(180deg,#f7a93a,#e8820a)}
  /* Live current-time marker: a soft line + dot head so it reads as "now", not a gap. */
  .dtrack .now{position:absolute;top:0;bottom:0;width:2px;background:rgba(255,255,255,.5)}
  .dtrack .now::after{content:"";position:absolute;top:3px;left:50%;transform:translateX(-50%);
    width:7px;height:7px;border-radius:50%;background:#fff;border:1.5px solid #02284c}
  .daxis{display:flex;justify-content:space-between;font-size:10px;color:var(--muted);
    margin-top:5px;font-variant-numeric:tabular-nums}
</style>
</head>
<body>
<header>
  <div class="titlewrap">
    <h1>Duravit <b>SensoWash</b> Bridge</h1>
    <div id="clock">—</div>
  </div>
</header>
<div id="pairbanner"></div>
<div class="grid" id="grid"></div>

<div id="logwrap">
  <div id="logbar"><span>Log</span>
    <span class="logbtns"><button id="log-clear">Clear</button><button id="log-toggle">Hide</button></span>
  </div>
  <div id="log"></div>
</div>

<div id="status">
  <span class="pill"><span class="dot" id="d-board"></span><span id="t-board">ESP Board offline</span></span>
  <span class="pill"><span class="bticon" id="bticon"><svg width="15" height="15" viewBox="0 0 24 24"><path fill="none" stroke="currentColor" stroke-width="2" stroke-linejoin="round" stroke-linecap="round" d="M8 7.5 16 16.5 12 20.5V3.5L16 7.5 8 16.5"/></svg></span><span id="t-ble">Bluetooth not linked</span></span>
  <span class="pill"><span id="t-build" class="build">v—</span></span>
  <span class="pill"><span id="t-uptime">Board online —</span></span>
  <span class="pill"><span id="t-conntime">Bluetooth link —</span></span>
</div>

<div class="modal" id="modal"><div class="sheet">
  <h3 id="m-title"></h3><p id="m-body"></p>
  <div class="btns"><button id="m-cancel">Cancel</button><button class="go" id="m-ok">Confirm</button></div>
</div></div>

<script>
"use strict";
var FALLBACK_OPTS = {
  "Night Light":["Off","On","Auto"],
  "Dryer":["Off","Low","Warm","Hot"],
  "Seat Temperature":["Off","Low","Warm","Hot"],
  "Water Flow":["Low","Medium","High"],
  "Nozzle Position":["1","2","3","4","5"],
  "Wash Water Temperature":["Off","Low","Medium","High"]
};
// Material Symbols icon paths (fonts.google.com/icons; viewBox 0 -960 960 960).
var IC = {
  volume:"M560-131v-82q90-26 145-100t55-168q0-94-55-168T560-749v-82q124 28 202 125.5T840-481q0 127-78 224.5T560-131ZM120-360v-240h160l200-200v640L280-360H120Zm440 40v-322q47 22 73.5 66t26.5 96q0 51-26.5 94.5T560-320ZM400-606l-86 86H200v80h114l86 86v-252ZM300-480Z",
  bt:"M440-80v-304L256-200l-56-56 224-224-224-224 56-56 184 184v-304h40l228 228-172 172 172 172L480-80h-40Zm80-496 76-76-76-74v150Zm0 342 76-74-76-76v150Z",
  auto:"M204-318q-22-38-33-78t-11-82q0-134 93-228t227-94h7l-64-64 56-56 160 160-160 160-56-56 64-64h-7q-100 0-170 70.5T240-478q0 26 6 51t18 49l-60 60ZM481-40 321-200l160-160 56 56-64 64h7q100 0 170-70.5T720-482q0-26-6-51t-18-49l60-60q22 38 33 78t11 82q0 134-93 228t-227 94h-7l64 64-56 56Z",
  wifi:"M480-120q-42 0-71-29t-29-71q0-42 29-71t71-29q42 0 71 29t29 71q0 42-29 71t-71 29ZM254-346l-84-86q59-59 138.5-93.5T480-560q92 0 171.5 35T790-430l-84 84q-44-44-102-69t-124-25q-66 0-124 25t-102 69ZM84-516 0-600q92-94 215-147t265-53q142 0 265 53t215 147l-84 84q-77-77-178.5-120.5T480-680q-116 0-217.5 43.5T84-516Z",
  days:"M580-240q-42 0-71-29t-29-71q0-42 29-71t71-29q42 0 71 29t29 71q0 42-29 71t-71 29ZM200-80q-33 0-56.5-23.5T120-160v-560q0-33 23.5-56.5T200-800h40v-80h80v80h320v-80h80v80h40q33 0 56.5 23.5T840-720v560q0 33-23.5 56.5T760-80H200Zm0-80h560v-400H200v400Zm0-480h560v-80H200v80Zm0 0v-80 80Z",
  warn:"m40-120 440-760 440 760H40Zm138-80h604L480-720 178-200Zm302-40q17 0 28.5-11.5T520-280q0-17-11.5-28.5T480-320q-17 0-28.5 11.5T440-280q0 17 11.5 28.5T480-240Zm-40-120h80v-200h-80v200Zm40-100Z",
  hw:"M360-360v-240h240v240H360Zm80-80h80v-80h-80v80Zm-80 320v-80h-80q-33 0-56.5-23.5T200-280v-80h-80v-80h80v-80h-80v-80h80v-80q0-33 23.5-56.5T280-760h80v-80h80v80h80v-80h80v80h80q33 0 56.5 23.5T760-680v80h80v80h-80v80h80v80h-80v80q0 33-23.5 56.5T680-200h-80v80h-80v-80h-80v80h-80Zm320-160v-400H280v400h400ZM480-480Z",
  sw:"M320-240 80-480l240-240 57 57-184 184 183 183-56 56Zm320 0-57-57 184-184-183-183 56-56 240 240-240 240Z",
  serial:"m240-160 40-160H120l20-80h160l40-160H180l20-80h160l40-160h80l-40 160h160l40-160h80l-40 160h160l-20 80H660l-40 160h160l-20 80H600l-40 160h-80l40-160H360l-40 160h-80Zm140-240h160l40-160H420l-40 160Z",
  rear:"M320-240q-17 0-28.5-11.5T280-280q0-17 11.5-28.5T320-320q17 0 28.5 11.5T360-280q0 17-11.5 28.5T320-240Zm160 0q-17 0-28.5-11.5T440-280q0-17 11.5-28.5T480-320q17 0 28.5 11.5T520-280q0 17-11.5 28.5T480-240Zm160 0q-17 0-28.5-11.5T600-280q0-17 11.5-28.5T640-320q17 0 28.5 11.5T680-280q0 17-11.5 28.5T640-240ZM200-400v-80q0-106 68-184t172-92v-84h80v84q104 14 172 92t68 184v80H200Zm80-80h400q0-83-58.5-141.5T480-680q-83 0-141.5 58.5T280-480Zm40 360q-17 0-28.5-11.5T280-160q0-17 11.5-28.5T320-200q17 0 28.5 11.5T360-160q0 17-11.5 28.5T320-120Zm160 0q-17 0-28.5-11.5T440-160q0-17 11.5-28.5T480-200q17 0 28.5 11.5T520-160q0 17-11.5 28.5T480-120Zm160 0q-17 0-28.5-11.5T600-160q0-17 11.5-28.5T640-200q17 0 28.5 11.5T680-160q0 17-11.5 28.5T640-120ZM480-480Z",
  lady:"M400-80v-240H280l122-308q10-24 31-38t47-14q26 0 47 14t31 38l122 308H560v240H400Zm80-640q-33 0-56.5-23.5T400-800q0-33 23.5-56.5T480-880q33 0 56.5 23.5T560-800q0 33-23.5 56.5T480-720Z",
  stop:"M320-320h320v-320H320v320ZM480-80q-83 0-156-31.5T197-197q-54-54-85.5-127T80-480q0-83 31.5-156T197-763q54-54 127-85.5T480-880q83 0 156 31.5T763-763q54 54 85.5 127T880-480q0 83-31.5 156T763-197q-54 54-127 85.5T480-80Zm0-80q134 0 227-93t93-227q0-134-93-227t-227-93q-134 0-227 93t-93 227q0 134 93 227t227 93Zm0-320Z",
  flow:"M80-146v-78q29 0 49.5-9t41.5-19.5q21-10.5 46.5-19T280-280q38 0 62.5 8.5t45.5 19q21 10.5 42 19.5t50 9q29 0 50-9t42-19.5q21-10.5 46-19t62-8.5q38 0 63 8.5t46 19q21 10.5 42 19.5t49 9v78q-38 0-63.5-9T770-174.5q-21-10.5-41-19t-49-8.5q-28 0-48.5 8.5t-41 19Q570-164 544.5-155t-64.5 9q-39 0-64.5-9t-46-19.5Q349-185 329-193.5t-49-8.5q-28 0-48.5 8.5t-41.5 19Q169-164 143.5-155T80-146Zm0-178v-78q29 0 49.5-9t41.5-19.5q21-10.5 46.5-19T280-458q38 0 62.5 8.5t45.5 19q21 10.5 42 19.5t50 9q29 0 50-9t42-19.5q21-10.5 46-19t62-8.5q38 0 63 8.5t46 19q21 10.5 42 19.5t49 9v78q-38 0-63.5-9T770-352.5q-21-10.5-41-19t-49-8.5q-29 0-49.5 8.5t-41 19Q569-342 544-333t-64 9q-39 0-64.5-9t-46-19.5Q349-363 329-371.5t-49-8.5q-28 0-48.5 8.5t-41.5 19Q169-342 143.5-333T80-324Zm0-178v-78q29 0 49.5-9t41.5-19.5q21-10.5 46.5-19T280-636q38 0 62.5 8.5t45.5 19q21 10.5 42 19.5t50 9q29 0 50-9t42-19.5q21-10.5 46-19t62-8.5q38 0 63 8.5t46 19q21 10.5 42 19.5t49 9v78q-38 0-63.5-9T770-530.5q-21-10.5-41-19t-49-8.5q-28 0-48.5 8.5t-41 19Q570-520 544.5-511t-64.5 9q-39 0-64.5-9t-46-19.5Q349-541 329-549.5t-49-8.5q-28 0-48.5 8.5t-41.5 19Q169-520 143.5-511T80-502Zm0-178v-78q29 0 49.5-9t41.5-19.5q21-10.5 46.5-19T280-814q38 0 62.5 8.5t45.5 19q21 10.5 42 19.5t50 9q29 0 50-9t42-19.5q21-10.5 46-19t62-8.5q38 0 63 8.5t46 19q21 10.5 42 19.5t49 9v78q-38 0-63.5-9T770-708.5q-21-10.5-41-19t-49-8.5q-28 0-48.5 8.5t-41 19Q570-698 544.5-689t-64.5 9q-39 0-64.5-9t-46-19.5Q349-719 329-727.5t-49-8.5q-28 0-48.5 8.5t-41.5 19Q169-698 143.5-689T80-680Z",
  nozpos:"M160-240q-33 0-56.5-23.5T80-320v-320q0-33 23.5-56.5T160-720h640q33 0 56.5 23.5T880-640v320q0 33-23.5 56.5T800-240H160Zm0-80h640v-320H680v160h-80v-160h-80v160h-80v-160h-80v160h-80v-160H160v320Zm120-160h80-80Zm160 0h80-80Zm160 0h80-80Zm-120 0Z",
  washtemp:"M520-520v-80h200v80H520Zm0-160v-80h320v80H520ZM320-120q-83 0-141.5-58.5T120-320q0-48 21-89.5t59-70.5v-240q0-50 35-85t85-35q50 0 85 35t35 85v240q38 29 59 70.5t21 89.5q0 83-58.5 141.5T320-120ZM200-320h240q0-29-12.5-54T392-416l-32-24v-280q0-17-11.5-28.5T320-760q-17 0-28.5 11.5T280-720v280l-32 24q-23 17-35.5 42T200-320Z",
  comfort:"M0-360v-240h80v240H0Zm120 80v-400h80v400h-80Zm760-80v-240h80v240h-80Zm-120 80v-400h80v400h-80ZM320-120q-33 0-56.5-23.5T240-200v-560q0-33 23.5-56.5T320-840h320q33 0 56.5 23.5T720-760v560q0 33-23.5 56.5T640-120H320Zm0-80h320v-560H320v560Zm0 0v-560 560Z",
  holiday:"M784-120 530-374l56-56 254 254-56 56Zm-546-28q-60-60-89-135t-29-153q0-78 29-152t89-134q60-60 134.5-89.5T525-841q78 0 152.5 29.5T812-722L238-148Zm8-122 54-54q-16-21-30.5-43T243-411q-12-22-21-44t-16-43q-11 59-1.5 118T246-270Zm112-110 222-224q-43-33-86.5-53.5t-81.5-28q-38-7.5-68.5-2.5T296-666q-17 18-22 48.5t2.5 69q7.5 38.5 28 81.5t53.5 87Zm278-280 56-54q-53-32-112-42t-118 2q22 7 44 16t44 20.5q22 11.5 43.5 26T636-660Z",
  clean:"M120-40v-280q0-83 58.5-141.5T320-520h40v-320q0-33 23.5-56.5T440-920h80q33 0 56.5 23.5T600-840v320h40q83 0 141.5 58.5T840-320v280H120Zm80-80h80v-120q0-17 11.5-28.5T320-280q17 0 28.5 11.5T360-240v120h80v-120q0-17 11.5-28.5T480-280q17 0 28.5 11.5T520-240v120h80v-120q0-17 11.5-28.5T640-280q17 0 28.5 11.5T680-240v120h80v-200q0-50-35-85t-85-35H320q-50 0-85 35t-35 85v200Zm320-400v-320h-80v320h80Zm0 0h-80 80Z",
  brush:"M440-520h80v-280q0-17-11.5-28.5T480-840q-17 0-28.5 11.5T440-800v280ZM200-360h560v-80H200v80Zm-58 240h98v-80q0-17 11.5-28.5T280-240q17 0 28.5 11.5T320-200v80h120v-80q0-17 11.5-28.5T480-240q17 0 28.5 11.5T520-200v80h120v-80q0-17 11.5-28.5T680-240q17 0 28.5 11.5T720-200v80h98l-40-160H182l-40 160Zm676 80H142q-39 0-63-31t-14-69l55-220v-80q0-33 23.5-56.5T200-520h160v-280q0-50 35-85t85-35q50 0 85 35t35 85v280h160q33 0 56.5 23.5T840-440v80l55 220q13 38-11.5 69T818-40Zm-58-400H200h560Zm-240-80h-80 80Z",
  self:"M491-200q12-1 20.5-9.5T520-230q0-14-9-22.5t-23-7.5q-41 3-87-22.5T343-375q-2-11-10.5-18t-19.5-7q-14 0-23 10.5t-6 24.5q17 91 80 130t127 35ZM480-80q-137 0-228.5-94T160-408q0-100 79.5-217.5T480-880q161 137 240.5 254.5T800-408q0 140-91.5 234T480-80Zm0-80q104 0 172-70.5T720-408q0-73-60.5-165T480-774Q361-665 300.5-573T240-408q0 107 68 177.5T480-160Zm0-320Z"
};
// Card layout. Each item references an entity by its exact friendly name.
var LAYOUT = [
  {title:"Lid & Night Light", items:[
    {type:"button", name:"Toilet Lid Toggle", display:"Open / Close Toilet lid", label:""},
    {type:"select", name:"Night Light"},
    {type:"switch", name:"Auto Lid Open"},
    {type:"switch", name:"Auto Lid Close"}
  ]},
  {title:"Dryer", items:[ {type:"select", name:"Dryer"} ]},
  {title:"Seat", items:[
    {type:"select", name:"Seat Temperature"},
    {type:"text", name:"Seat Occupied", label:"Seat", bool:true, on:"Occupied", off:"Vacant"},
    {type:"switch", name:"Energy Saving", spaced:true},
    {type:"schedule"}
  ]},
  {title:"Washing", items:[
    {type:"button", name:"Rear Wash", label:"Start", icon:IC.rear},
    {type:"button", name:"Lady Wash", label:"Start", icon:IC.lady},
    {type:"button", name:"Stop Wash", display:"Stop", label:"", icon:IC.stop, redicon:true},
    {type:"select", name:"Water Flow"},
    {type:"select", name:"Nozzle Position"},
    {type:"select", name:"Wash Water Temperature"},
    {type:"switch", name:"Comfort Wash"}
  ]},
  {title:"Maintenance", items:[
    {type:"button", name:"Nozzle Self-Clean", label:"Start", icon:IC.self},
    {type:"button", name:"Nozzle Manual Clean", label:"Start", icon:IC.brush},
    {type:"button", name:"Holiday Mode (Drain Tank)", label:"Start", icon:IC.holiday,
      confirm:"Drain the internal tank for holiday mode?"},
    {type:"button", name:"⚠️ Start Descaling", display:"Start Descaling", label:"Start", icon:IC.clean,
      confirm:"Start descaling?\nThis needs descaling tablets in place and runs a long cycle."}
  ]},
  {title:"Settings", items:[
    {type:"switch", name:"Feedback Tones", icon:IC.volume},
    {type:"bleconn"},
    {type:"switch", name:"Auto Bluetooth Reconnect", greyWhenDisc:true, icon:IC.auto},
    {type:"signal", name:"WiFi Signal", label:"WiFi signal", icon:IC.wifi},
    {type:"signal", name:"Toilet BLE Signal", label:"Bluetooth signal", icon:IC.bt},
    {type:"value", name:"Descaling Remaining", label:"Days to descale", suffix:" days", icon:IC.days},
    {type:"text", name:"Descaling State", label:"Descaling", icon:IC.clean},
    {type:"text", name:"Fault Codes", label:"Faults", fault:true, icon:IC.warn},
    {type:"text", name:"Hardware Version", label:"Hardware", icon:IC.hw},
    {type:"text", name:"Software Version", label:"Software", icon:IC.sw},
    {type:"text", name:"Serial Number", label:"Serial", icon:IC.serial}
  ]}
];

// Fault catalog: error number -> [service code, type, fix]. Transcribed from the Duravit APK
// (ErrorCode enum + strings.xml). The firmware publishes the active error NUMBERS as CSV.
var FD = {
  POWER:"Switch off the power supply. Call the installer.",
  SEAT:"Switch off the power supply. Call the installer.",
  DRYER:"Switch off the power supply. Call the installer.",
  WATER:"Close the water supply. Close the stop valve. Call the installer.",
  WATER18:"Option 1: clean the water filter, check the tubes for kinks, ensure the water supply is open.\nOption 2 (if that fails): switch off the power, close the stop valve, call the installer.",
  GEAR:"Call the installer.",
  FLUSH:"Switch off the power supply. Close the stop valve. Call the installer."
};
var FAULTS = {
  1:["721.1","Power supply",FD.POWER],2:["721.2","Power supply",FD.POWER],3:["721.3","Power supply",FD.POWER],
  4:["725.1","Seat heating",FD.SEAT],9:["725.2","Seat heating",FD.SEAT],10:["725.3","Seat heating",FD.SEAT],
  11:["725.4","Seat heating",FD.SEAT],12:["725.6","Seat heating",FD.SEAT],13:["725.5","Seat heating",FD.SEAT],
  17:["735.1","Water supply",FD.WATER],18:["735.2","Water supply",FD.WATER18],19:["735.3","Water supply",FD.WATER],20:["735.4","Water supply",FD.WATER],
  25:["726.2","Shower water temperature",FD.WATER],26:["726.3","Shower water temperature",FD.WATER],27:["726.5","Shower water temperature",FD.WATER],28:["726.6","Shower water temperature",FD.WATER],29:["726.1","Shower water temperature",FD.WATER],30:["726.7","Shower water temperature",FD.WATER],
  33:["727.1","Warm air dryer",FD.DRYER],34:["727.2","Warm air dryer",FD.DRYER],35:["727.3","Warm air dryer",FD.DRYER],
  41:["722.1","Seat / lid gearbox",FD.GEAR],42:["722.2","Seat / lid gearbox",FD.GEAR],
  49:["746.1","HygieneUV","HygieneUV: fan abnormal."],50:["746.2","HygieneUV","HygieneUV: voltage exceeds the set value."],51:["746.3","HygieneUV","HygieneUV: no module feedback 3x."],
  65:["738.3","Flushing system",FD.FLUSH]
};
function faultBody(csv){
  return csv.split(",").map(function(x){return parseInt(x,10);}).filter(function(n){return !isNaN(n);}).map(function(n){
    var f=FAULTS[n]||["?","Unknown error "+n,"Unrecognised code "+n+" reported by the toilet."];
    return "⚠ "+f[1]+"\nService code "+f[0]+"\n"+f[2];
  }).join("\n\n———————————\n\n");
}
function openFault(csv){
  ask("Toilet fault detected",faultBody(csv),true,function(){});
  document.getElementById("m-ok").style.display="none";
  document.getElementById("m-cancel").textContent="Close";
}

var REG = {};        // name -> {update, oid, domain}
var idIndex = {};    // "domain-oid" -> name
function reg(name){ if(!REG[name]) REG[name]={}; return REG[name]; }

function post(path){ fetch(path,{method:"POST"}).catch(function(){}); }
// Entity-name REST URLs (the object-id form is deprecated; removed in ESPHome 2026.7.0).
function url(domain,name,tail){ return "/"+domain+"/"+encodeURIComponent(name)+tail; }

// ---- modal ----
var modal=document.getElementById("modal");
function ask(title,body,danger,cb){
  document.getElementById("m-title").textContent=title;
  document.getElementById("m-body").textContent=body;
  var ok=document.getElementById("m-ok");
  ok.style.display=""; ok.className=danger?"danger":"go";          // reset (fault modal hides OK)
  ok.onclick=function(){modal.classList.remove("show");cb();};
  var cx=document.getElementById("m-cancel"); cx.textContent="Cancel";
  cx.onclick=function(){modal.classList.remove("show");};
  modal.classList.add("show");
}

// ---- builders ----
function el(tag,cls,txt){var e=document.createElement(tag);if(cls)e.className=cls;if(txt!=null)e.textContent=txt;return e;}
// Inline Material Symbols icon (path d -> <svg class=ic>). currentColor fills it.
var SVGNS="http://www.w3.org/2000/svg";
function svgIcon(d){
  var s=document.createElementNS(SVGNS,"svg"); s.setAttribute("class","ic"); s.setAttribute("viewBox","0 -960 960 960");
  var p=document.createElementNS(SVGNS,"path"); p.setAttribute("d",d); s.appendChild(p); return s;
}
// Row label that optionally leads with a Material icon (settings rows).
function rowLabel(item,cls,text){
  if(!item.icon) return el("span",cls,text);
  var w=el("span",cls+" wico"); w.appendChild(svgIcon(item.icon)); w.appendChild(el("span",null,text)); return w;
}

// Functions called whenever the toilet BLE connection state changes (for the combined
// connect/disconnect button and greying the auto-reconnect toggle when disconnected).
var bleConnHooks = [];

function buildSwitch(item){
  var row=el("div","row"+(item.spaced?" spaced":"")); row.appendChild(rowLabel(item,"lbl",item.label||item.name));
  var tg=el("div","toggle"); row.appendChild(tg);
  tg.onclick=function(){ if(!row.classList.contains("disabled")) post(url("switch",item.name,"/toggle")); };
  reg(item.name).update=function(d){ tg.classList.toggle("on",(d.value===true)||(d.state==="ON")); };
  if(item.greyWhenDisc) bleConnHooks.push(function(c){ row.classList.toggle("disabled",!c); });
  return row;
}

// Single button that toggles between Connect / Disconnect based on connection state (no label tag).
function buildBleConn(){
  var b=el("button","act"); var lead=el("span","lead"); lead.appendChild(svgIcon(IC.bt));
  var lbl=el("span",null,"Connect Bluetooth"); lead.appendChild(lbl); b.appendChild(lead);
  var connected=false;
  b.onclick=function(){ if(!connected){ connectingShow=true; connectShownAt=Date.now(); }
    post(url("button", connected?"Disconnect Bluetooth":"Reconnect Bluetooth","/press")); };
  bleConnHooks.push(function(c){ connected=c; lbl.textContent=c?"Disconnect Bluetooth":"Connect Bluetooth";
    b.classList.toggle("discon",c); });
  return b;
}

// Static informational note (e.g. the energy-saving schedule).
function buildNote(item){ var d=el("div","note"); d.innerHTML=item.html; return d; }

function buildSelect(item){
  var wrap=el("div","seg"); wrap.appendChild(el("div","lbl",item.label||item.name));
  var opts=FALLBACK_OPTS[item.name]||[]; var btns={};
  opts.forEach(function(opt){
    var b=el("button",null,opt);
    b.onclick=function(){ post(url("select",item.name,"/set?option="+encodeURIComponent(opt))); };
    btns[opt]=b; wrap.appendChild(b);
  });
  reg(item.name).update=function(d){
    var cur=(d.value!=null)?String(d.value):d.state;
    for(var o in btns) btns[o].classList.toggle("sel", o===cur);
  };
  return wrap;
}

function buildButton(item){
  var b=el("button","act"+(item.danger?" danger":"")+(item.big?" big":""));
  var lead=el("span","lead");
  if(item.icon){ var ic=svgIcon(item.icon); if(item.redicon) ic.style.color="var(--red)"; lead.appendChild(ic); }
  lead.appendChild(el("span",null,item.display||item.name));
  b.appendChild(lead);
  if(item.label!=="") b.appendChild(el("span","go",item.label||"Go"));  // label:"" => no right-side pill
  b.onclick=function(){
    var fire=function(){ post(url("button",item.name,"/press")); };
    if(item.confirm) ask(item.display||item.name,item.confirm,item.danger,fire); else fire();
  };
  return b;
}

// Energy-saving day calendar: a 24h timeline with the seat-heating-on (warm) windows highlighted,
// eco (heating off) elsewhere, plus a live "now" marker. Windows mirror the firmware preset.
var WARM_WINDOWS=[[6,9],[19,22]];   // hours the seat heating is ON
function buildDaybar(){
  var wrap=el("div","daycal");
  var leg=el("div","leg");
  leg.innerHTML='<span><i class="warm"></i>Seat heating on</span><span><i class="eco"></i>Eco — heating off</span>';
  wrap.appendChild(leg);
  var track=el("div","dtrack");
  WARM_WINDOWS.forEach(function(w){
    var s=el("div","warm");
    s.style.left=(w[0]/24*100)+"%"; s.style.width=((w[1]-w[0])/24*100)+"%";
    var lbl=("0"+w[0]).slice(-2)+":00–"+("0"+w[1]).slice(-2)+":00";
    s.title="Seat heating on "+lbl;
    track.appendChild(s);
  });
  var now=el("div","now"); track.appendChild(now);
  function tick(){ var d=new Date(); now.style.left=((d.getHours()*60+d.getMinutes())/1440*100)+"%"; }
  tick(); setInterval(tick,60000);
  wrap.appendChild(track);
  var ax=el("div","daxis");
  ["00:00","06:00","12:00","18:00","24:00"].forEach(function(t){ ax.appendChild(el("span",null,t)); });
  wrap.appendChild(ax);
  return wrap;
}

// Action button that drives a switch's turn_on/turn_off (e.g. BLE connect/disconnect).
function buildCmd(item){
  var b=el("button","act");
  b.appendChild(el("span",null,item.label));
  b.appendChild(el("span","go","Go"));
  b.onclick=function(){ post(url("switch",item.sw,"/"+item.act)); };
  return b;
}

// Read-only text row (optionally boolean-formatted or fault-highlighted).
function buildText(item){
  var row=el("div","srow"); row.appendChild(rowLabel(item,"k",item.label||item.name));
  var v=el("span","v","—"); row.appendChild(v);
  reg(item.name).update=function(d){
    if(item.bool){ var on=(d.value===true)||(d.state==="ON"); v.textContent=on?(item.on||"Yes"):(item.off||"No"); return; }
    var s=(d.value!=null&&d.value!=="")?String(d.value):(d.state||"");
    if(item.fault){
      var err=s && !/^(no|none)$/i.test(s) && s!=="0";
      v.textContent=err?"Fault detected":"None"; v.classList.toggle("warnv",!!err);
      v.onclick=err?function(){openFault(s);}:null;
      if(err && !item._prevErr) openFault(s);   // auto pop-up when a fault first appears
      item._prevErr=err;
    } else { v.textContent=s||"—"; }
  };
  return row;
}

// Numeric value row with a suffix.
function buildValue(item){
  var row=el("div","srow"); row.appendChild(rowLabel(item,"k",item.label||item.name));
  var v=el("span","v","—"); row.appendChild(v);
  reg(item.name).update=function(d){ v.textContent=(d.value!=null&&d.value!=="")?(d.value+(item.suffix||"")):"—"; };
  return row;
}

// Signal row: phone-style bars + dBm number.
function sigBars(dbm){ if(dbm==null)return 0; if(dbm>=-55)return 4; if(dbm>=-67)return 3; if(dbm>=-78)return 2; if(dbm>=-88)return 1; return 0; }
function buildSignal(item){
  var row=el("div","srow"); row.appendChild(rowLabel(item,"k",item.label||item.name));
  var sig=el("span","sig"), bars=el("span","bars"), bi=[];
  for(var i=1;i<=4;i++){ var b=el("i","b"+i); bars.appendChild(b); bi.push(b); }
  var num=el("span","v","—"); sig.appendChild(bars); sig.appendChild(num); row.appendChild(sig);
  reg(item.name).update=function(d){
    var dbm=(d.value!=null&&d.value!=="")?Math.round(d.value):null, lvl=sigBars(dbm);
    bars.classList.toggle("weak",dbm!=null&&lvl<=1);
    for(var i=0;i<4;i++) bi[i].classList.toggle("on", i<lvl);
    num.textContent=(dbm!=null)?(dbm+" dBm"):"—";
  };
  return row;
}

// ---- render layout ----
var grid=document.getElementById("grid");
LAYOUT.forEach(function(card){
  var c=el("div","card"); c.appendChild(el("h2",null,card.title));
  card.items.forEach(function(it){
    if(it.type==="switch") c.appendChild(buildSwitch(it));
    else if(it.type==="select") c.appendChild(buildSelect(it));
    else if(it.type==="button") c.appendChild(buildButton(it));
    else if(it.type==="bleconn") c.appendChild(buildBleConn());
    else if(it.type==="note") c.appendChild(buildNote(it));
    else if(it.type==="schedule") c.appendChild(buildDaybar());
    else if(it.type==="text") c.appendChild(buildText(it));
    else if(it.type==="value") c.appendChild(buildValue(it));
    else if(it.type==="signal") c.appendChild(buildSignal(it));
  });
  grid.appendChild(c);
});

// ---- bottom-bar connection state + live timers ----
function fmtDur(s){ s=Math.max(0,Math.floor(s));
  var d=Math.floor(s/86400); s-=d*86400; var h=Math.floor(s/3600); s-=h*3600;
  var m=Math.floor(s/60); s-=m*60;
  return (d?d+"d ":"")+((h||d)?h+"h ":"")+((m||h||d)?m+"m ":"")+s+"s"; }
var bleConnected=false, upBase=0, upAt=0, connBase=0, connAt=0, sseUp=false, pairingState="", connectingShow=false, connectShownAt=0;
function applyBleConn(){ bleConnHooks.forEach(function(h){ h(bleConnected); }); }
reg("Toilet Connected").update=function(d){
  bleConnected=(d.value===true)||(d.state==="ON");
  if(bleConnected) connectingShow=false;
  document.getElementById("bticon").classList.toggle("on",bleConnected);
  document.getElementById("t-ble").textContent=bleConnected?"Bluetooth linked":"Bluetooth not linked";
  applyBleConn();
};
reg("Build Version").update=function(d){
  var v=d.value||d.state||""; if(v) document.getElementById("t-build").textContent="v"+v;
};
// Enrolment banner: only appears when the firmware is waiting for the toilet's Bluetooth button.
reg("Pairing Status").update=function(d){
  var v=(d.value!=null&&d.value!=="")?String(d.value):(d.state||"");
  pairingState=v;
  var b=document.getElementById("pairbanner");
  if(v==="awaiting_key"){
    b.className="show"; b.innerHTML="🔵 <b>Pairing</b> — press the Bluetooth button on your toilet now…";
  } else if(v==="paired" && b._wasAwaiting){
    b.className="show ok"; b.textContent="✅ Paired!";
    setTimeout(function(){ b.className=""; },4000);
  } else { b.className=""; }
  b._wasAwaiting=(v==="awaiting_key");
};
reg("ESP Board Uptime").update=function(d){ if(d.value!=null){upBase=+d.value;upAt=Date.now();} };
reg("Toilet Connected Time").update=function(d){ if(d.value!=null){connBase=+d.value;connAt=Date.now();} };
setInterval(function(){
  // Freeze the counters when the SSE link is down so a stale/backgrounded page can't show
  // "online" while the indicator says offline.
  document.getElementById("t-uptime").textContent=(sseUp&&upAt)?("Board online "+fmtDur(upBase+(Date.now()-upAt)/1000)):"Board online —";
  var ct=document.getElementById("t-conntime");
  if(sseUp&&bleConnected&&connAt) ct.textContent="Bluetooth link "+fmtDur(connBase+(Date.now()-connAt)/1000);
  else if(sseUp&&(pairingState==="connecting"||pairingState==="awaiting_key"||(connectingShow&&(Date.now()-connectShownAt<90000)))) ct.textContent="Connecting…";
  else { connectingShow=false; ct.textContent="Bluetooth link —"; }
},1000);

// ---- Log panel ----
var logEl=document.getElementById("log"), LOGCAP=250;
function logLine(txt){
  if(!txt) return;
  txt=txt.replace(/\x1b\[[0-9;]*m/g,"");                       // strip ANSI colour codes
  var cls="l";
  if(/\bTX\b/.test(txt)) cls="tx"; else if(/\bRX\b/.test(txt)) cls="rx";
  else if(/\[W\]/.test(txt)||/WARN/.test(txt)) cls="w";
  else if(/\[E\]/.test(txt)||/ERROR/.test(txt)) cls="e";
  var atBottom=(logEl.scrollHeight-logEl.scrollTop-logEl.clientHeight)<30;
  var line=document.createElement("div"); line.className=cls; line.textContent=txt;
  logEl.appendChild(line);
  while(logEl.childNodes.length>LOGCAP) logEl.removeChild(logEl.firstChild);
  if(atBottom) logEl.scrollTop=logEl.scrollHeight;
}
document.getElementById("log-clear").onclick=function(){logEl.textContent="";};
document.getElementById("log-toggle").onclick=function(){
  logEl.classList.toggle("hidden");
  this.textContent=logEl.classList.contains("hidden")?"Show":"Hide";
};

// ---- SSE wiring ----
function handle(d){
  if(!d||!d.id) return;
  var name=d.name||idIndex[d.id];
  if(d.name){ idIndex[d.id]=d.name; var i=d.id.indexOf("-");
    var r=reg(d.name); r.domain=d.id.slice(0,i); r.oid=d.id.slice(i+1);
    if(d.option&&FALLBACK_OPTS[d.name]==null) FALLBACK_OPTS[d.name]=d.option; }
  if(!name) return;
  var rr=REG[name]; if(rr&&rr.update) rr.update(d);
}
function boardUp(up){
  sseUp=up;
  document.getElementById("d-board").className="dot "+(up?"on":"off");
  document.getElementById("t-board").textContent=up?"ESP Board online":"Reconnecting…";
  if(!up){ bleConnected=false; document.getElementById("bticon").classList.remove("on");
    document.getElementById("t-ble").textContent="Bluetooth not linked"; applyBleConn(); }
}
applyBleConn();  // initial state: show "Connect Bluetooth" + grey the auto toggle until connected
var es=null;
function connect(){
  if(es){ try{ es.onopen=es.onerror=es.onmessage=null; es.close(); }catch(x){} }  // drop any stale stream first
  es=new EventSource("/events");
  es.onopen=function(){boardUp(true);};
  es.onerror=function(){boardUp(false);};
  es.addEventListener("state",function(e){try{handle(JSON.parse(e.data));}catch(x){}});
  es.addEventListener("log",function(e){ logLine(typeof e.data==="string"?e.data:""); });
  es.onmessage=function(e){ if(e.data&&e.data.charAt(0)==="{"){ try{var d=JSON.parse(e.data);if(d&&d.id)handle(d);}catch(x){} } };
}
// On resume (tab refocus / page restored from bfcache / network back), force a fresh stream + state
// pull instead of trusting a possibly-suspended one — fixes the "offline but counter ticking" stale page.
document.addEventListener("visibilitychange",function(){ if(!document.hidden) connect(); });
window.addEventListener("pageshow",connect);
window.addEventListener("online",connect);
// Live clock under the title (viewer-local time + timezone).
function tickClock(){
  var d=new Date(), tz="", abbr="";
  try{ tz=Intl.DateTimeFormat().resolvedOptions().timeZone; }catch(e){}
  try{ abbr=d.toLocaleTimeString("en-GB",{timeZoneName:"short"}).split(" ").pop(); }catch(e){}
  var t=d.toLocaleTimeString("en-GB",{hour:"2-digit",minute:"2-digit",second:"2-digit"});
  var ds=d.toLocaleDateString("en-GB",{weekday:"short",day:"2-digit",month:"short"});
  document.getElementById("clock").textContent=ds+"  "+t+"  "+tz+(abbr?" ("+abbr+")":"");
}
setInterval(tickClock,1000); tickClock();
connect();
</script>
</body>
</html>
)DASH";

bool DashboardHandler::canHandle(AsyncWebServerRequest *request) const {
  if (request->method() != HTTP_GET)
    return false;
  char buf[AsyncWebServerRequest::URL_BUF_SIZE];
  StringRef url = request->url_to(buf);
  return url == "/" || url == "/index.html" || url == "/fonts.css" || url == "/bg.jpg" || url == "/bgp.jpg";
}

void DashboardHandler::handleRequest(AsyncWebServerRequest *request) {
  char buf[AsyncWebServerRequest::URL_BUF_SIZE];
  StringRef url = request->url_to(buf);
  if (url == "/fonts.css") {
    // Brand font as a separate, immutably-cached stylesheet so the main page stays lean.
    auto *res = request->beginResponse(200, "text/css; charset=utf-8", (const uint8_t *) FONTS_CSS,
                                       sizeof(FONTS_CSS) - 1);
    res->addHeader("Cache-Control", "public, max-age=31536000, immutable");
    request->send(res);
    return;
  }
  if (url == "/bg.jpg" || url == "/bgp.jpg") {
    bool port = (url == "/bgp.jpg");
    auto *res = request->beginResponse(200, "image/jpeg", port ? BG_PORT : BG_LAND,
                                       port ? BG_PORT_LEN : BG_LAND_LEN);
    res->addHeader("Cache-Control", "public, max-age=31536000, immutable");
    request->send(res);
    return;
  }
  request->send(200, "text/html; charset=utf-8", DASHBOARD_HTML);
}

void WebDashboard::setup() {
  this->base_->add_handler(new DashboardHandler());  // NOLINT(cppcoreguidelines-owning-memory)
}

void WebDashboard::dump_config() {
  ESP_LOGCONFIG(TAG, "Web Dashboard: custom UI registered at '/'");
}

}  // namespace web_dashboard
}  // namespace esphome
#endif  // USE_ESP32
