document.addEventListener('DOMContentLoaded', function () {
    const temperatureElement = document.getElementById('temperature');
    const humidityElement = document.getElementById('humidity');
    const soilMoistureElement = document.getElementById('soil-moisture');
    const co2LevelElement = document.getElementById('co2-level');

    function fetchSensorData() {
        fetch('/sensor_data')
            .then(response => response.json())
            .then(data => {
                temperatureElement.textContent = '\u{1f321} ' + data.temperature + ' \xB0C';
                humidityElement.textContent = '\u{1f32b} ' + data.humidity + ' %';
                soilMoistureElement.textContent = '\u{1f4a7} ' + data.soilMoisture + ' %';
                co2LevelElement.textContent = '\u{1fa9f} ' + data.co2Level + ' ppm';
            })
            .catch(error => {
                console.error('Error fetching sensor data:', error);
                temperatureElement.textContent = 'Error';
                humidityElement.textContent = 'Error';
                soilMoistureElement.textContent = 'Error';
                co2LevelElement.textContent = 'Error';
            });
    }

    fetchSensorData();
    setInterval(fetchSensorData, 2000);
});
