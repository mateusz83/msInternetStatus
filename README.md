# msInternetStatus

---------------------------
--- Available on Aminet ---
---------------------------
http://aminet.net/package/comm/net/msInternetStatus

----------------
--- Overview ---
----------------

msInternetStatus is a small commodity tool that monitors the Internet
connection in realtime by performing simple, fast and non-blocking
TCP connection attempt to given IP adress.

The connection status:
- is always saved into system global ENV variable named "msInternetStatus"
- additionally can be displayed as text or colored rectangle.

--------------------
--- Installation ---
--------------------

msInternetStatus doesn't require installation. 

To start the program just double-click at the msInternetStatus icon. 
Double-click again to turn it off. To always start the program at system startup,
move msInternetStatus icon into SYS:WBStartup folder.

Because msInternetStatus is a commodity tool it also can be controlled 
by the Exchange tool located in SYS:Tools/Commodities folder.

------------------
--- How to use ---
------------------

By default, after the first start, msInternetStatus shows the Online/Offline
status on the title bar of a small window in the top-left corner of the screen.

But you can configure msInternetStatus on your own 
and change its appearance using tool types.

After changing any tool type, to see the results you need to restart the program 
by double-clicking on it or using Exchange tool.

   #1. Displaying the status on Workbench menu bar using ENV variable.

   If you are AmigaOS 3.2 user, the best way to use msInternetStatus is to display
   the status directly on Workbench menu bar by using ENV variable "msInternetStatus"
   that is set by the program. To do this, go to your Workbench preferences
   in SYS:Prefs/Workbench. Edit the "Screen Title Format" label by adding:
   %emsInternetStatus and clicking Use or Save. You also probably want to close
   the default status window by setting tool type CX_POPUP=NO.

   For non AmigaOS 3.2 users, you can add "msInternetStatus" ENV variable
   to WB title bar by using additional tools like for example MCP.


   #2. Displaying the status in LABEL mode.

   This mode allows user to display the status on Workbench without using the ENV
   variable. It displays the label with current status with the same text
   and backgorund color as Workbench bar.Set tooltype MODE=LABEL, you should also
   set the position using POS_X= and POS_Y= tooltypes.If the label is too long
   and overlays something, you can adjust SIZE_X= tooltype.


   #3 Displaying status as color rectangle in BOX mode.

   This mode allows to visualize the status not as text but as color box
   for example green when online and red when offline.
   Set tooltype MODE=BOX, adjust position using POS_X= and POS_Y=,
   also you can adjust box non regular size SIZE_X= and SIZE_Y= or leave it
   in default values.


   #4 Displaying the status in small window in WINDOW_BAR mode.

   This mode is set by default. It shows the status on title bar of a small window.
   To use it, set tool type MODE=WINDOW_BAR. You can adjust position by using
   POS_X= and POS_Y= tool types.

--------------------------------------------
--- Tool types configurations and options ---
--------------------------------------------

Tool types are editable options saved in the programs icon. To access and edit them, 
select msInternetStatus icon, right-click to open WB menu bar and select 
Icons->Information... Under tool types list you will find following options:

After changing any tooltype, to see the results you need to restart the program 
by double-clicking on it or using Exchange tool.

   `CX_PRIORITY=0`
   Sets the commodity priority. Can be set to =0

  ` CX_POPUP=YES`
   Should the status window popup after start? Use =YES or =NO

 `  PRIMARY_IP=216.58.213.0`
   First IP to check (google.com).

   `SECONDARY_IP=1.1.1.1`
   Second IP to check if the first will fail (cloudflare.com).

   `TIME_INTERVAL=5`
   How often program checks the connection in seconds. Can be =2..3600

   `TCP_TIMEOUT=1`
   How many seconds program will wait for response from IP. Can be =1..5

   MODE=WINDOW_BAR
   The way the status is displayed, if the window is visible. 
   Can be =LABEL or =BOX or =WINDOW_BAR (all explained in 'How to Use' section)

 `  ONLINE_TXT=Online`
   What text should appear when the status in online.
   It can be any string, for example =ONLINE or =We have the NET.
   This option is used for MODE=LABEL and MODE=WINDOW_BAR

 `  OFFLINE_TXT=Offline`
   Same as above but for offline status.

   `BOX_ONLINE_COLOR=3`
   What color the box should have when the status is online.
   If the value =0..255 it is used as PEN number. 
   If the value is in R,G,B format, for example =255,0,0
   the program will try to obtain this color or similar if available.
   Used in MODE=BOX.

`   BOX_OFFLINE_COLOR=1`
   Same as above but for offline.

`   POS_X=30 and POS_Y=30`
   The starting coordinates of the status window, if visible.
   The status widnow can be displayed anywhere on the WB screen.
   Used in all modes: MODE=LABEL MODE=BOX MODE=WINDOW_BAR.

`   SIZE_X=0`
   For MODE=LABEL value =0 will set the width to text length but not precise. 
   So values >0 can be used to fine tune the width and avoid overlapping.
   FOR MODE=BOX value =0 will set the width to default value. Use >0 to set 
   your own size. For MODE=WINDOW_BAR this value is not used.

  ` SIZE_Y=0`
   FOR MODE=BOX value =0 will set the height to default value. Use >0 to 
   set your own size. For MODE=LABEL and MODE=WINDOW_BAR this value is not used.

`   DEBUG=0`
   In case of connection problems you can set this value to =1. 
   The output window will appear with detailed connection informations. 

----------------
--- Examples ---
----------------

A couple of example settings of tool types for getting different visual results.
Check the "examples" folder to see .iff images.

After changing any tooltype, to see the results you need to restart the program 
by double-clicking on it or using Exchange tool.

   EXAMPLE #1. 
   Wide and thin bar with green/black status, placed under WB title bar.
   (example values for for 640x512 screen size)

      CX_POPUP=YES
      MODE=BOX
      BOX_ONLINE_COLOR=0,255,0
      BOX_OFFLINE_COLOR=1
      POS_X=570
      POS_Y=30
      SIZE_X=60
      SIZE_Y=8

   In this example we are using RGB values to get desired green color for online
   status. System will try to allocate that pen colors or select the closest match.
   Should work if your system is in 8 colors or more. For offline status we are
   using system pen no. 1 which is black.

   EXAMPLE #2.
   The text saying "Your are online!" or "No connection!" 
   is displayed on the right side of WB title bar like beeing a natural part of it.
   (example values for for 640x512 screen size)

      CX_POPUP=YES
      MODE=LABEL
      ONLINE_TXT=You are online!
      OFFLINE_TXT=No connection!
      POS_X=520
      POS_Y=5
      SIZE_X=90

   In this example we used SIZE_X=90 instead of SIZE_X=0, to shrink the background
   of the label.Auto size mode is not precise and can waste the space and overlap
   other elements.      

   EXAMPLE #3.
   Checking the status of ENV variable.

   The "msInternetStatus" global ENV variable is always updated despite selected MODE.
   Go to the System Shell or right-click and choose Workbench->Execute Command.. 
   from WB menu bar and type:
      
      echo $msInternetStatus

----------------
--- Testing ----
----------------

msInternetStatus was successfully tested with:

- 3.1, 3.2, 3.5 systems (AGA/RTG)
- ZZ9000 card with Roadshow TCP/IP stack
- Ariadne card with Miami TCP/IP stack
- X-Surf 100 card with AmiTCP TCP/IP stack
- V1200 card with Roadshow TCP/IP stack
- WinUAE


``
