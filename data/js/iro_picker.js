const colorPicker = new iro.ColorPicker('#color', {
    width: screen.width * 0.8,
    color: "rgb(255, 0, 0)",
    borderWidth: 2,
    borderColor: "#fff",
    wheelLightness: false,
    sliderSize: (screen.width * 0.8) / 11,
});
colorPicker.color.value = 0;

let lastColor = "rgb(0, 0, 0)";

setInterval(function () {
    if (!document.querySelector('#colorfx').checked && lastColor !== colorPicker.color.rgbString) {
        socket.send(JSON.stringify({
            action: "set_colors",
            data: {
                r: colorPicker.color.rgb.r,
                g: colorPicker.color.rgb.g,
                b: colorPicker.color.rgb.b,
            }
        }));
        lastColor = colorPicker.color.rgbString;
    }
}, 100);