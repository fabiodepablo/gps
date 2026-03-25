const API_URL = 'http://localhost:3000/api/track';

// Inicializar Mapa centrado en una ubicación por defecto (Ej: Madrid o Global)
const map = L.map('map', {
    zoomControl: false // Quitamos control default para reubicarlo si queremos
}).setView([40.4168, -3.7038], 5);

// Add custom zoom control position
L.control.zoom({ position: 'bottomright' }).addTo(map);

// Tiles del mapa (OpenStreetMap CartoDB Dark Matter)
L.tileLayer('https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png', {
    attribution: '&copy; <a href="https://www.openstreetmap.org/copyright">OSM</a> &copy; <a href="https://carto.com/">CARTO</a>',
    subdomains: 'abcd',
    maxZoom: 19
}).addTo(map);

const speedEl = document.getElementById('speed-val');
const timeEl = document.getElementById('time-val');
const devEl = document.getElementById('dev-val');
const indicatorEl = document.getElementById('status-indicator');
const refreshBtn = document.getElementById('refresh-btn');
const deviceSelect = document.getElementById('device-select');

let selectedDevice = 'ALL';

deviceSelect.addEventListener('change', (e) => {
    selectedDevice = e.target.value;
    refreshBtn.innerText = "Sincronizando...";
    fetchTrackerData().then(() => refreshBtn.innerText = "Sincronizar Datos");
});

// Variables para la ruta
let routeLine = L.polyline([], { 
    color: '#00d2ff', 
    weight: 4, 
    opacity: 0.8,
    className: 'custom-poly-line'
}).addTo(map);

const markers = {}; // Diccionario para guardar marcadores por dispositivo

async function fetchTrackerData() {
    try {
        indicatorEl.classList.remove('offline');
        
        let data = [];
        try {
            // Consulta para tener el panorama global actual de la flota
            const response = await fetch(API_URL.replace('/track', '/devices/latest'));
            data = await response.json();
        } catch(e) {
            console.log('Backend no accesible. Mostrando datos simulados UI.');
            data = [
                { DeviceName: 'Camion-01', Latitude: 40.4168 + (Math.random()*0.01), Longitude: -3.7038 + (Math.random()*0.01), Speed: Math.floor(Math.random()*80), Timestamp: new Date().toISOString() },
                { DeviceName: 'Moto-04', Latitude: 40.4500, Longitude: -3.7500, Speed: 60, Timestamp: new Date().toISOString() }
            ];
        }

        if (data && data.length > 0) {
            
            // LLenar el selector dinamicamente si aparecen dispositivos nuevos
            data.forEach(d => {
                if(![...deviceSelect.options].some(opt => opt.value === d.DeviceName)) {
                    deviceSelect.add(new Option(d.DeviceName, d.DeviceName));
                }
            });

            // Actualizar Marcadores Globales
            const bounds = [];
            data.forEach(device => {
                const latlng = [device.Latitude, device.Longitude];
                bounds.push(latlng);
                
                if (!markers[device.DeviceName]) {
                    markers[device.DeviceName] = L.marker(latlng, {icon: carIcon})
                        .bindPopup(`<b>${device.DeviceName}</b><br>Velocidad: ${Math.round(device.Speed)} km/h`)
                        .addTo(map);
                } else {
                    markers[device.DeviceName].setLatLng(latlng);
                    markers[device.DeviceName].setPopupContent(`<b>${device.DeviceName}</b><br>Velocidad: ${Math.round(device.Speed)} km/h`);
                }
            });

            if (selectedDevice === 'ALL') {
                // Modo Flota
                routeLine.setLatLngs([]); // Limpiamos la linea
                speedEl.innerHTML = `<span class="number">${data.length}</span> activos`;
                devEl.innerText = data.length > 1 ? "Todos los vehiculos" : data[0].DeviceName;
                const dateObj = new Date(data[0].Timestamp);
                timeEl.innerText = dateObj.toLocaleTimeString([], {hour: '2-digit', minute:'2-digit', second:'2-digit'});
                
                // Centrar en todos suavemente si no hay interactividad reciente (opcional)
                // if(bounds.length > 0) map.fitBounds(bounds, {padding: [50, 50]});

            } else {
                // Modo Individual
                const specData = data.find(d => d.DeviceName === selectedDevice);
                if (specData) {
                    // Update Stats
                    speedEl.innerHTML = `<span class="number">${Math.round(specData.Speed)}</span> km/h`;
                    devEl.innerText = specData.DeviceName;
                    const dateObj = new Date(specData.Timestamp);
                    timeEl.innerText = dateObj.toLocaleTimeString([], {hour: '2-digit', minute:'2-digit', second:'2-digit'});
                    
                    // Fetch historial especifico para trazar linea de color neon (routeLine)
                    try {
                        let historyData;
                        try {
                            const resHist = await fetch(API_URL.replace('/track', '/track/' + selectedDevice));
                            historyData = await resHist.json();
                        } catch(er) {
                            historyData = [
                                specData,
                                { Latitude: specData.Latitude - 0.005, Longitude: specData.Longitude - 0.005 }
                            ];
                        }
                        
                        const pts = historyData.map(d => [d.Latitude, d.Longitude]).reverse();
                        routeLine.setLatLngs(pts);
                        map.setView([specData.Latitude, specData.Longitude], 15);
                    } catch(histErr) {
                        console.error('Error dibujando ruta:', histErr);
                    }
                }
            }
        }

    } catch (err) {
        console.error('Error fetching tracker data', err);
        indicatorEl.classList.add('offline');
        devEl.innerText = 'Error red';
    }
}

// Eventos
refreshBtn.addEventListener('click', () => {
    refreshBtn.innerText = "Sincronizando...";
    fetchTrackerData().then(() => {
        setTimeout(() => refreshBtn.innerText = "Sincronizar Datos", 500);
    });
});

// Auto fetch periodicamente (10s)
fetchTrackerData();
setInterval(fetchTrackerData, 10000);
