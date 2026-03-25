const express = require('express');
const cors = require('cors');
const sql = require('mssql');

const app = express();
app.use(cors());
app.use(express.json());

// Configuracion de SQL Server (Cambiar con tus credenciales reales)
const dbConfig = {
    user: 'sa', // Tu usuario
    password: 'tu_password', // Tu contrasena
    server: 'localhost', // Servidor de base de datos
    database: 'GPS_Tracking', // Nombre de la DB
    options: {
        encrypt: true, // true para Azure, false para servidores locales sin SSL
        trustServerCertificate: true // importante para localhost developer
    }
};

// Intentar iniciar pull de conexiones al arrancar
let poolPromise = sql.connect(dbConfig)
    .then(pool => {
        console.log('Conectado a SQL Server correctamente');
        return pool;
    })
    .catch(err => {
        console.error('Error al iniciar conexion SQL Server: ', err);
    });

// Ruta para recibir datos del ESP32 SIM808
app.post('/api/track', async (req, res) => {
    try {
        const { name, lat, lng, speed } = req.body;

        if (!name || isNaN(lat) || isNaN(lng)) {
            return res.status(400).json({ error: 'Datos no validos. Se requiere name, lat, y lng' });
        }

        const pool = await poolPromise;
        const result = await pool.request()
            .input('name', sql.NVarChar, name)
            .input('latitude', sql.Decimal(10, 6), lat)
            .input('longitude', sql.Decimal(10, 6), lng)
            .input('speed', sql.Float, speed || 0)
            // Query asume que existe la tabla Locations: CREATE TABLE Locations (Id INT IDENTITY(1,1), DeviceName NVARCHAR(100), Latitude DECIMAL(10,6), Longitude DECIMAL(10,6), Speed FLOAT, Timestamp DATETIME DEFAULT GETDATE())
            .query('INSERT INTO Locations (DeviceName, Latitude, Longitude, Speed, Timestamp) VALUES (@name, @latitude, @longitude, @speed, GETDATE())');

        console.log(`[POST] Recibido dato de ${name} (Lat: ${lat}, Lng: ${lng})`);
        res.status(200).json({ message: 'Ubicacion guardada' });
    } catch (err) {
        console.error('Error procesando POST /api/track', err);
        res.status(500).json({ error: 'Error del servidor' });
    }
});

// Ruta para obtener la lista de dispositivos y su ultima ubicacion (Para mapa general)
app.get('/api/devices/latest', async (req, res) => {
    try {
        const pool = await poolPromise;
        // Consulta para SQL Server que trae la ultima ubicacion por dispositivo
        const query = `
            SELECT DeviceName, Latitude, Longitude, Speed, Timestamp 
            FROM (
                SELECT *, ROW_NUMBER() OVER(PARTITION BY DeviceName ORDER BY Timestamp DESC) as rn 
                FROM Locations
            ) t 
            WHERE t.rn = 1
        `;
        const result = await pool.request().query(query);
        res.status(200).json(result.recordset);
    } catch (err) {
        console.error('Error en GET /api/devices/latest', err);
        
        // Mock fallback para pruebas en UI si no hay base real aun:
        res.status(200).json([
            { DeviceName: 'Camion-01', Latitude: 40.4168, Longitude: -3.7038, Speed: 45, Timestamp: new Date().toISOString() },
            { DeviceName: 'Moto-04', Latitude: 40.4500, Longitude: -3.7500, Speed: 60, Timestamp: new Date().toISOString() },
            { DeviceName: 'Auto-11', Latitude: 40.3800, Longitude: -3.6800, Speed: 30, Timestamp: new Date(Date.now() - 50000).toISOString() }
        ]);
    }
});

// Ruta original para obtener historial de UN dispositivo especifico (para dibujar la linea roja)
app.get('/api/track/:deviceName', async (req, res) => {
    try {
        const pool = await poolPromise;
        const result = await pool.request()
            .input('device', sql.NVarChar, req.params.deviceName)
            .query('SELECT TOP 50 * FROM Locations WHERE DeviceName = @device ORDER BY Timestamp DESC');
        res.status(200).json(result.recordset);
    } catch (err) {
        // Fallback mock
        res.status(200).json([
            { DeviceName: req.params.deviceName, Latitude: 40.4168, Longitude: -3.7038, Speed: 40, Timestamp: new Date().toISOString() },
            { DeviceName: req.params.deviceName, Latitude: 40.4150, Longitude: -3.7000, Speed: 45, Timestamp: new Date(Date.now() - 10000).toISOString() }
        ]);
    }
});

const PORT = 3000;
app.listen(PORT, () => {
    console.log(`API Corriendo en http://localhost:${PORT}`);
    console.log(`El ESP32 debe mandar un HTTP POST a http://<IP_DE_ESTE_PC>:${PORT}/api/track`);
});
