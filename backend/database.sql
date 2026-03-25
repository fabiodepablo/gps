-- Script para ejecutar en SQL Server (SSMS)
-- 1. Crear la base de datos
CREATE DATABASE GPS_Tracking;
GO

USE GPS_Tracking;
GO

-- 2. Crear la tabla de ubicaciones
CREATE TABLE Locations (
    Id INT IDENTITY(1,1) PRIMARY KEY,
    DeviceName NVARCHAR(100) NOT NULL,
    Latitude DECIMAL(10,6) NOT NULL,
    Longitude DECIMAL(10,6) NOT NULL,
    Speed FLOAT NOT NULL,
    Timestamp DATETIME DEFAULT GETDATE()
);
GO

-- 3. (Opcional) Indices para busquedas mas rapidas por dispositivo o fecha
CREATE INDEX IX_Locations_Timestamp ON Locations(Timestamp DESC);
CREATE INDEX IX_Locations_DeviceName ON Locations(DeviceName);
GO
