from ftplib import FTP

# Connect to the server
ftp = FTP()
ftp.connect('localhost', 2121)
ftp.login(user='anyuser', passwd='anypassword')

# Switch to passive mode and execute LIST
ftp.set_pasv(True)
print("Directory listing:")
ftp.retrlines('LIST')

ftp.quit()
