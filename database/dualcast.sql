CREATE DATABASE lora_db;

USE lora_db;

CREATE TABLE messages (
    id INT AUTO_INCREMENT PRIMARY KEY,
    mac VARCHAR(20),
    rssi INT,
    message TEXT,
    timestamp DATETIME
);