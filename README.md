# FTP Server

This is a simple FTP server implemented in C++. It supports a variety of FTP commands to interact with files and directories. Below are the specifications and functionality of each supported command.

---

## **Supported FTP Commands**

### **1. USER**
- **Description**: Specifies the username for authentication.
- **Usage**: `USER <username>`
- **Response**:
  - `331 Username okay, need password.`: Prompt for password after username is entered.
  - `530 Invalid username.`: If the username is invalid.

---

### **2. PASS**
- **Description**: Specifies the password for authentication.
- **Usage**: `PASS <password>`
- **Response**:
  - `230 User logged in, proceed.`: If the username and password are correct.
  - `503 Bad sequence of commands.`: If `USER` was not issued before `PASS`.
  - `530 Invalid password.`: If the password is incorrect.

---

### **3. QUIT**
- **Description**: Terminates the FTP session.
- **Usage**: `QUIT`
- **Response**:
  - `221 Goodbye.`: Server acknowledges the termination of the session.

---

### **4. PWD**
- **Description**: Prints the current working directory.
- **Usage**: `PWD`
- **Response**:
  - `257 "<directory>" is the current directory.`: Returns the path to the current directory.

---

### **5. CWD**
- **Description**: Changes the working directory.
- **Usage**: `CWD <directory>`
- **Response**:
  - `250 Directory successfully changed.`: If the directory change is successful.
  - `550 Failed to change directory.`: If the specified directory does not exist or cannot be accessed.

---

### **6. MKD**
- **Description**: Creates a new directory.
- **Usage**: `MKD <directory>`
- **Response**:
  - `257 "<directory>" directory created.`: If the directory is successfully created.
  - `550 Failed to create directory.`: If the directory could not be created.

---

### **7. TYPE**
- **Description**: Sets the transfer mode (ASCII or Binary).
- **Usage**: `TYPE <mode>` (where `<mode>` is `A` for ASCII or `I` for Binary)
- **Response**:
  - `200 Type set successfully.`: If the transfer mode is successfully set.
  - `504 Command not implemented for that parameter.`: If an unsupported mode is specified.

---

### **8. SIZE**
- **Description**: Returns the size of a specified file.
- **Usage**: `SIZE <filename>`
- **Response**:
  - `213 <size>`: If the file exists, returns its size in bytes.
  - `550 File not found.`: If the file does not exist.

---

### **9. MDTM**
- **Description**: Returns the last modification time of a file.
- **Usage**: `MDTM <filename>`
- **Response**:
  - `213 <timestamp>`: Returns the file's last modification time in `YYYYMMDDHHMMSS` format.
  - `550 File not found.`: If the file does not exist.

---

### **10. PORT**
- **Description**: Specifies the client's data connection port.
- **Usage**: `PORT <h1,h2,h3,h4,p1,p2>` (where `<h1-h4>` is the client IP and `<p1,p2>` is the port)
- **Response**:
  - `200 PORT command successful.`: If the data connection is successfully configured.
  - `425 Can't open data connection.`: If the server fails to connect to the specified port.

---

### **11. PASV**
- **Description**: Switches to passive mode and provides the server's IP and port for data transfer.
- **Usage**: `PASV`
- **Response**:
  - `227 Entering Passive Mode (<h1,h2,h3,h4,p1,p2>).`: Returns the server's IP and port for data transfer.
  - `425 Can't open data connection.`: If the server fails to create a passive data connection.

---

### **12. LIST**
- **Description**: Lists files and directories in the current working directory.
- **Usage**: `LIST`
- **Response**:
  - `150 Opening data connection for directory listing.`: Starts transferring the directory listing.
  - `226 Directory send OK.`: After the directory listing is sent successfully.
  - `425 Use PORT or PASV first.`: If no data connection is established.
  - `450 Requested file action not taken. Directory unavailable.`: If the directory does not exist.

---

### **13. STOR**
- **Description**: Uploads a file to the server.
- **Usage**: `STOR <filename>`
- **Response**:
  - `150 Opening data connection.`: Starts receiving the file.
  - `226 Transfer complete.`: When the file is successfully uploaded.
  - `425 Use PASV first.`: If no data connection is established.
  - `426 Connection closed; transfer aborted.`: If the transfer fails.
  - `550 Could not create file.`: If the file could not be created on the server.

---

### **14. RETR**
- **Description**: Downloads a file from the server.
- **Usage**: `RETR <filename>`
- **Response**:
  - `150 Opening data connection.`: Starts sending the file.
  - `226 Transfer complete.`: When the file is successfully sent.
  - `425 Use PASV first.`: If no data connection is established.
  - `426 Connection closed; transfer aborted.`: If the transfer fails.
  - `550 File not found or access denied.`: If the file does not exist or cannot be accessed.

---

### **15. NOOP**
- **Description**: Does nothing; used to keep the connection alive.
- **Usage**: `NOOP`
- **Response**:
  - `200 Command okay.`: Confirms that the server is responsive.

---

## **File System Structure**
- **Root Directory**: The root directory of the FTP server is the `storage` folder, where all files and directories are stored.
- **Temporary Files**: Temporary files created during transfers are automatically cleaned up.

---

## **How to Use**
1. Start the server:
   ```bash
   ./ftp-server
