#pragma once

// ─── Captive Portal HTML Template ────────────────────────
// Styled dark theme matching the ISS Tracker aesthetic.
// Placeholders:
//   %NETWORKS%  — replaced with <option> tags from WiFi scan
//   %MESSAGE%   — replaced with status/error messages (or empty)
// ─────────────────────────────────────────────────────────

const char PORTAL_HTML_TEMPLATE[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
<title>ISS Tracker Setup</title>
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    background: #0a0a0a; color: #fff;
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
    min-height: 100vh;
    display: flex; justify-content: center; align-items: flex-start;
    padding: 50px 20px;
  }
  .wrap { max-width: 380px; width: 100%; }

  /* Branding */
  .brand { text-align: center; margin-bottom: 36px; }
  .brand h1 {
    color: #00e5ff; font-size: 22px; letter-spacing: 6px;
    font-weight: 300; margin-bottom: 8px;
  }
  .brand .accent {
    width: 50px; height: 2px; background: #FED500;
    margin: 0 auto; border-radius: 1px;
  }
  .brand p { color: #555; font-size: 13px; margin-top: 14px; }

  /* Form elements */
  label {
    color: #777; font-size: 12px; letter-spacing: 2px;
    display: block; margin: 22px 0 6px;
  }
  select, input[type=password], input[type=text] {
    width: 100%; padding: 14px 12px;
    background: #151515; border: 1px solid #2a2a2a;
    color: #fff; border-radius: 10px; font-size: 16px;
    outline: none; transition: border-color 0.2s;
  }
  select:focus, input:focus { border-color: #00e5ff; }
  #manual_ssid { display: none; margin-top: 8px; }

  /* Submit button */
  button {
    width: 100%; padding: 16px; margin-top: 32px;
    background: #00e5ff; color: #000; border: none;
    border-radius: 10px; font-size: 15px; font-weight: 600;
    letter-spacing: 2px; cursor: pointer;
    transition: background 0.15s;
  }
  button:active { background: #00b8d4; transform: scale(0.98); }

  /* Messages */
  .msg {
    text-align: center; margin-top: 18px; font-size: 14px;
    padding: 12px; border-radius: 8px;
  }
  .msg.error { color: #ff6b6b; background: #1a0808; border: 1px solid #331111; }

  .footer { text-align: center; margin-top: 44px; color: #2a2a2a; font-size: 11px; }
</style>
</head>
<body>
<div class="wrap">
  <div class="brand">
    <h1>ISS TRACKER</h1>
    <div class="accent"></div>
    <p>WiFi Configuration</p>
  </div>

  <form action="/save" method="POST">
    <label>NETWORK</label>
    <select name="ssid" id="ssid"
      onchange="document.getElementById('manual_ssid').style.display=this.value==='__manual__'?'block':'none'">
      %NETWORKS%
    </select>
    <input type="text" name="manual_ssid" id="manual_ssid" placeholder="Enter network name (SSID)">

    <label>PASSWORD</label>
    <input type="password" name="pass" placeholder="Enter WiFi password">

    <button type="submit">CONNECT</button>
  </form>

  %MESSAGE%

  <p class="footer">ISS Tracker v1.0</p>
</div>
</body>
</html>
)rawliteral";
