/*
   This software is licensed under the MIT License. See the license file for details.
   Source: https://github.com/spacehuhntech/WiFiDuck

   Modified and adapted by:
    - Dereck81
 */
// ===== WebSocket Actions ===== //
function ws_connected() {}

// ===== Startup ===== //
window.addEventListener("load", function() {
  const prohibited = ["sd_ls", "sd_cat", "sd_stream_write", "sd_stream_write_begin", "key", "key_ack"];

  E("send").onclick = function() {
    var input = E("input").value;

    var cmdKey = input.split(" ")[0].toLowerCase();

    if (prohibited.includes(cmdKey)) {
      E("output").innerHTML += `<b style="color:red">[BLOCK] The command '${cmdKey}' it is not allowed from the terminal.</b><br>`;
      E("output").scrollTop = E("output").scrollHeight;
      E("input").value = "";
      return;
    }

    E("output").innerHTML += "# " + input + "<br>";

    E("reconnect").onclick = ws_init;

    ws_send(input, function(msg) {
      log(msg);
      E("output").innerHTML += msg.replace(/\n/g, "<br>");
      E("output").scrollTop = E("output").scrollHeight;
    });
  };

  E("clear").onclick = function() {
    E("output").innerHTML = "";
  };

  ws_init();
}, false);