# Tagger-UI

Tagger-UI is a GUI suite for the [tagger](https://github.com/cedricfrancoys/tagger) command-line tool, and consists of 3 utilities allowing respectively to tag files, search among tagged files, and monitor file system changes to update tagger database.  

In addition, Tagger-UI for Windows comes along with a full featured NSI installer that sets the environment and: 
* Adds a 'tag file...' entry to the shell context menu
* Adds start menu shortcut to tfsearch.exe utility
* Optionally, sets tfmon.exe to run automatically at startup

## tftag.exe ##

For adding or removing tag(s) to any file from the file system.
 
 
![tftag](https://cloud.githubusercontent.com/assets/2885156/13174695/c67fb162-d705-11e5-8282-bd4ead49449e.jpg)
![tftag - menu](https://cloud.githubusercontent.com/assets/2885156/13174693/c666a0c8-d705-11e5-8fb5-59cbe6f07445.jpg)

## tfsearch.exe  ##

App for searching among tagged files. 
 
 
![tfsearch](https://cloud.githubusercontent.com/assets/2885156/13174691/c6287b0e-d705-11e5-96ff-88ca542bcacb.jpg)




## tfmon.exe ##

This optional tool is a filesystem monitoring daemon allowing to maintain tagger database consistency when a tagged file is moved, renamed or deleted.  
Supports fixed drives, logical drives and mapped drives.
 
![tfmon](https://cloud.githubusercontent.com/assets/2885156/13174692/c64d6d74-d705-11e5-9921-8ad63785b2a1.jpg)


