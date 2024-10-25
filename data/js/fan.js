let lastFan = 0;

setInterval(function () {
    const slider = document.querySelector('.slider');
    const percentage = document.querySelector('.percentage');

    percentage.textContent = slider.value + '%';

    if (slider.value !== lastFan) {
        socket.send(JSON.stringify({
            action: "set_fan",
            data: {
                fan: Number(slider.value)
            }
        }));
        lastFan = slider.value;
    }
}, 1000);