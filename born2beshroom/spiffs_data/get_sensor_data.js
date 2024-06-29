document.addEventListener('DOMContentLoaded', function () {
    const temperatureElement = document.getElementById('temperature');
    const humidityElement = document.getElementById('humidity');
    const soilMoistureElement = document.getElementById('soil-moisture');
    const co2LevelElement = document.getElementById('co2-level');

    function fetchSensorData() {
        fetch('/sensor_data')
            .then(response => response.json())
            .then(data => {
                temperatureElement.textContent = data.temperature + ' \xB0C';
                humidityElement.textContent = data.humidity + ' %';
                soilMoistureElement.textContent = data.soilMoisture + ' %';
                co2LevelElement.textContent = data.co2Level + ' ppm';
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
    setInterval(fetchSensorData, 5000);
});
