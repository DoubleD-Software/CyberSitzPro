let temp = 0;
let humi = 0;

let lastColorFX = false;
let lastAssOver = false;

setInterval(function () {
    const colorFX = document.querySelector('#colorfx').checked;
    if (!colorFX && colorFX !== lastColorFX) {
        socket.send(JSON.stringify({
            action: "set_colors",
            data: {
                r: colorPicker.color.rgb.r,
                g: colorPicker.color.rgb.g,
                b: colorPicker.color.rgb.b,
            }
        }));
    }
    if (colorFX !== lastColorFX) {
        socket.send(JSON.stringify({
            action: "set_color_fx",
            data: {
                rainbow: colorFX
            }
        }));
        lastColorFX = colorFX;
    }
    const assOver = document.querySelector('#assover').checked;
    if (assOver !== lastAssOver) {
        socket.send(JSON.stringify({
            action: "set_ass_override",
            data: {
                override: assOver
            }
        }));
        lastAssOver = assOver;
    }
}, 500);