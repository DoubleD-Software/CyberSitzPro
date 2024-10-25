let socket = new WebSocket(WEBSOCKET_ADDRESS);
const ass_threshold = 27;

socket.onopen = function (event) {
	console.log("WS Connection established");
	socket.send(JSON.stringify({
		action: "get_sensor_values",
	}));
};

socket.onmessage = function (event) {
	const json = JSON.parse(event.data);
	console.log(json);
	if (json["type"] === "sensor_values") {
		const tempObj = document.querySelector('.temp');
		const humiObj = document.querySelector('.humi');
		console.log(json);
		if (json["data"]["temperature"] != null) {
			temp = json["data"]["temperature"].toFixed(1);
			tempObj.textContent = temp + "°C/" + ass_threshold + "°C";
		}
		if (json["data"]["humidity"] != null) {
			humi = json["data"]["humidity"];
			humiObj.textContent = humi + "%";
		}
		if (json["data"]["colorfx"] != null) {
			document.querySelector('#colorfx').checked = json["data"]["colorfx"];
		}
		if (json["data"]["assover"] != null) {
			document.querySelector('#assover').checked = json["data"]["assover"];
		}
		if (json["data"]["fan"] != null) {
			document.querySelector('.slider').value = json["data"]["fan"];
			document.querySelector('.percentage').textContent = json["data"]["fan"] + "%";
		}
		if (json["data"]["r"] != null && json["data"]["g"] != null && json["data"]["b"] != null) {
			colorPicker.color.red = json["data"]["r"];
			colorPicker.color.green = json["data"]["g"];
			colorPicker.color.blue = json["data"]["b"];
		}
	}
};

socket.onclose = function (event) {
	if (event.wasClean) {
		console.log("WS Connection closed cleanly");
	} else {
		console.log("WS Connection died");
	}
};

socket.onerror = function (error) {
	console.log("WS Connection error");
};