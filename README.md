For Mac users that work from their Downloads directory:

Run sudo mv filemgr /usr/local/bin/mgr (or whatever name you want) so that you can run filemgr from anywhere, including your Downloads directory.

Run mgr (or whatever name you choose) to see the available list of commands and their corresponding actions.

In order to run the upload command, make sure you have rclone installed -> "brew install rclone"

Run "rclone config" and follow the config instructions, note to also make sure to name the project "gdrive", otherwise update the code in "file_upload.cpp" to correspond. Note that upload is only for google drives, so configure with that in mind. If you rebuild, make sure to remake. 

Folders that are not made by filemgr will not be manipulated in anyway, so if you have files that you don't want touched, put them in a "PROTECTED" folder. 