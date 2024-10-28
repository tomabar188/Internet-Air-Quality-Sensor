# ESP32 IoT Air Quality Monitoring Project
This project uses an ESP32 development board to measure PM10 and PM2.5 particulate levels, which are then sent to a Node.js server and stored in a MySQL database. The data can be visualized through an EJS-powered web interface with real-time updates displayed on an LED matrix.

![image](https://github.com/user-attachments/assets/8524fd9f-27ac-4211-b015-72a90457efe1)
![image](https://github.com/user-attachments/assets/ca994182-a93e-4cce-aee5-68f42be21c3f)

# Project Structure
### 1. ESP32 Code
`main.c`: Manages WiFi connection, sensor data handling, and HTTP communication with the Node.js server.  
`ledmatrix7219.c` and `ledmatrix7219.h`: Control the LED matrix to display PM10 and PM2.5 values using SPI.
### 2. Node.js Server
node-server.js:
Hosts the backend server using Express.
Establishes a connection to a MySQL database for data storage.  
Provides endpoints for:  
`/check-connection`: Verifies connection with the ESP32.  
`/post`: Receives sensor data from the ESP32.  
`/all`: Fetches all records.  
`/search`: Filters records based on criteria.  
`/datalist`: Displays a list of measurements.  
Database Setup: Ensure a MySQL database with tables for PM10 and PM2.5 measurements, including fields for timestamped records.
### 3. Frontend
index.ejs, datalist.ejs, style.ejs, script.ejs: Provides a web interface for viewing air quality data, with filtering options for recent measurements.
### 4. Configuration Files
`package.json & package-lock.json`: Manage Node.js dependencies, including express, mysql, and body-parser.  
`platformio.ini`: Configures PlatformIO for ESP32 development.  
`sdkconfig.esp32doit-devkit-v1`: ESP32 SDK configuration file.  

### Set up the MySQL database:

Create a MySQL database named `espsensor` and tables for `PM10` and `PM2.5` with fields for `date` and `time`.


## Usage
The ESP32 device connects to the WiFi and starts measuring air quality data.
Data is sent to the Node.js server, where it's stored in the MySQL database.
The web interface provides real-time updates and historical data for analysis.
