/* ---------------------------------------------------------
 * msInternetStatus v0.1
 * by Mateusz Staniszew, 2023
 * 
 * The Internet status monitor wrapped as commodity tool.
 * In simple way checks TCP connection to given IP adress
 * and save the result as system ENV variable.
 * 
 * This code is provided without any warranty.
 * The author is not liable for anything bad 
 * that might happen as a result of the code.
 * ---------------------------------------------------------*/

#include <proto/exec.h>
#include <proto/dos.h>
#include <clib/alib_protos.h>

#include <clib/commodities_protos.h>
#include <libraries/commodities.h>

#include <proto/icon.h>

#include <proto/intuition.h>
#include <proto/gadtools.h>
#include <intuition/intuition.h>

#include <proto/graphics.h>

#include <sys/socket.h>
#include <proto/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>

#include <stdio.h>

// Application name and version.
#define   APP_NAME            "msIntenetStatus"
#define   APP_VERSION         "v0.2"
#define   APP_DESCRIPTION     "Checks Internet connection status."   
#define   APP_ENV_NAME        "msInternetStatus"

// Setting bsdsocket version to 3, if 4 some TCP/IP stacks like EasyNet wont work.
#define   APP_BSDSOCKET_LIB_VERSION     3

// Version that appears in Icon->Information.
static const char  *APP_info_version =	"$VER: Version "APP_VERSION;

// Libs.
struct Library*     CxBase         = NULL;
struct Library*     SocketBase     = NULL;
struct Library*     IconBase       = NULL;

// Handlers to public screen and visual info.
struct Screen *APP_pubscreen;
APTR APP_visual_info;

// Window (optional)
struct Window  *APP_window;

// Default input arguments (in case of problems).
#define   DEF_CX_PRIORITY          0
#define   DEF_CX_POPUP             "YES"
#define   DEF_PRIMARY_IP           "216.58.213.0"
#define   DEF_SECONDARY_IP         "1.1.1.1"
#define   DEF_TIME_INTERVAL        5
#define   DEF_TCP_TIMEOUT          1
#define   DEF_MODE                 "WINDOW_BAR"
#define   DEF_ONLINE_TXT           "Online"
#define   DEF_OFFLINE_TXT          "Offline"
#define   DEF_BOX_ONLINE_COLOR     "3"
#define   DEF_BOX_OFFLINE_COLOR    "1"
#define   DEF_POS_X                30
#define   DEF_POS_Y                30
#define   DEF_SIZE_X               0
#define   DEF_SIZE_Y               0
#define   DEF_BOX_SIZE             15
#define   DEF_DEBUG                0

// Assign values to MODES.
enum { MODE_LABEL, MODE_BOX, MODE_WINDOW_BAR };

// Input arguments holders.
BYTE   arg_cx_popup, arg_mode, arg_debug;
LONG   arg_time_interval, arg_tcp_timeout;
LONG   arg_pos_x, arg_pos_y, arg_size_x, arg_size_y;
LONG   arg_box_online_pen, arg_box_offline_pen;
STRPTR arg_primary_ip, arg_secondary_ip, arg_online_txt, arg_offline_txt;
STRPTR arg_box_online_color, arg_box_offline_color;

// Other variables.
BYTE   APP_window_visible;
LONG   APP_primary_ip_converted, APP_secondary_ip_converted;
LONG   APP_status_longest_strlen;

// For IP status
#define IP_STATUS_FAILED       0
#define IP_STATUS_CONNECTED    1
#define IP_STATUS_NOT_USED    -1

BYTE   APP_primary_ip_status, APP_secondary_ip_status;
UBYTE  APP_debug_count;

// Commodity globals.
struct NewBroker cx_newbroker = 
{
    NB_VERSION, 					// nb_Version - Version of the NewBroker structure
    (STRPTR)APP_NAME,		          // nb_Name - Name CX uses to identify this commodity
    (STRPTR)APP_NAME" "APP_VERSION,	// nb_Title - Title of commodity that appears in CXExchange
    (STRPTR)APP_DESCRIPTION,            // nb_Descr - Description of the commodity
    NBU_UNIQUE | NBU_NOTIFY,			// nb_Unique - Tells CX not to launch new commodity with same name
    COF_SHOW_HIDE,  				// nb_Flags - Tells CX if this commodity has a window
    0,          					// nb_Pri - This commodity's priority
    0,          					// nb_Port - MsgPort CX talks to
    0           					// nb_ReservedChannel - reserved for later use
};

struct MsgPort*     cx_broker_message_port;
CxObj*              cx_broker;

// Timer globals.
struct MsgPort*     timer_message_port;
struct timerequest* timer_io;

// Helper functions.
void Timer_Send(ULONG _sec, ULONG _micro)
{
     timer_io->tr_node.io_Command 	= TR_ADDREQUEST;

     timer_io->tr_time.tv_micro	= _micro;
     timer_io->tr_time.tv_sec      = 0;     
     timer_io->tr_time.tv_secs    	= _sec;
     timer_io->tr_time.tv_usec     = 0;
     
     SendIO((struct IORequest *)timer_io);
}
int  Timer_Init()
{
	timer_message_port = CreateMsgPort();
	if (timer_message_port == NULL)
		return 0;

	timer_io = (struct timerequest*)CreateExtIO(timer_message_port, sizeof(struct timerequest));
	if (timer_io == NULL)
		return 0;

     // Avoid CheckIO() hanging bug (???).
	timer_io->tr_node.io_Message.mn_Node.ln_Type = 0;

	if (OpenDevice(TIMERNAME, UNIT_VBLANK, (struct IORequest *)timer_io, 0L))
          return 0;

     Timer_Send(0, 1);

     return 1;
}
void Timer_Cleanup()
{
     if (timer_io)
     {
          // All I/O requests must be complete before CloseDevice().
          AbortIO(timer_io); 

          // Clean up.
          WaitIO(timer_io);
          CloseDevice( (struct IORequest*) timer_io);     
          DeleteExtIO( (struct IORequest*) timer_io);
     }

	if (timer_message_port) DeleteMsgPort(timer_message_port);
}

BYTE Intuition_Window_Create(void)
{
     // Try get public screen handler.
     APP_pubscreen = LockPubScreen(NULL);
     if (APP_pubscreen == NULL) return 0;

     // Validate values according to selected mode and size.
     switch(arg_mode)
     {
          case MODE_LABEL:
               if (arg_size_x == 0) arg_size_x = APP_pubscreen->RastPort.TxWidth * APP_status_longest_strlen;
               arg_size_y = APP_pubscreen->RastPort.TxHeight;
               break;
          
          case MODE_BOX:
               if (arg_size_x == 0) arg_size_x = DEF_BOX_SIZE;
               if (arg_size_y == 0) arg_size_y = DEF_BOX_SIZE;

               // Additionally - 
               // if we are in BOX mode and user provided RGB color value instead of pen number,
               // lets try to obtain best pen.
               LONG r, g, b;

               // Online pen.
               if (strlen(arg_box_online_color) <= 3)
               {
                    arg_box_online_pen = atoi(arg_box_online_color);
                    if (arg_box_online_pen < 0 || arg_box_online_pen > 255) arg_box_online_pen = atoi(DEF_BOX_ONLINE_COLOR);
               }
               else
               {
                    sscanf(arg_box_online_color, "%d,%d,%d", &r, &g, &b);
                    r <<= 24;
                    g <<= 24;
                    b <<= 24;
   
                    arg_box_online_pen = ObtainBestPen(APP_pubscreen->ViewPort.ColorMap, r, g, b, OBP_FailIfBad, FALSE, OBP_Precision, PRECISION_GUI, TAG_END);
               }

               // Offline pen.
               if (strlen(arg_box_offline_color) <= 3)
               {
                    arg_box_offline_pen = atoi(arg_box_offline_color);
                    if (arg_box_offline_pen < 0 || arg_box_offline_pen > 255) arg_box_offline_pen = atoi(DEF_BOX_OFFLINE_COLOR);
               }
               else
               {
                    sscanf(arg_box_offline_color, "%d,%d,%d", &r, &g, &b);
                    r <<= 24;
                    g <<= 24;
                    b <<= 24;
                    
                    arg_box_offline_pen = ObtainBestPen(APP_pubscreen->ViewPort.ColorMap, r, g, b, OBP_FailIfBad, FALSE, OBP_Precision, PRECISION_GUI, TAG_END);
               }               
               break;

          case MODE_WINDOW_BAR:
               if (arg_size_x == 0) arg_size_x = (APP_pubscreen->RastPort.TxWidth + 2) * APP_status_longest_strlen + 20;
               arg_size_y = 0;
               break;
     }

 	// --- Create window ---
     if (arg_mode == MODE_LABEL || arg_mode == MODE_BOX)
     {
          APP_window = OpenWindowTags(NULL,  WA_Left, arg_pos_x, 
                                             WA_Top, arg_pos_y, 
                                             WA_Width, arg_size_x, 
                                             WA_Height, arg_size_y, 
                                             WA_PubScreen, (ULONG)APP_pubscreen,                                             
                                             WA_Flags, WFLG_BORDERLESS, 
                                             TAG_END);
     }
     else
     {
          APP_window = OpenWindowTags(NULL,  WA_Left, arg_pos_x, 
                                             WA_Top, arg_pos_y, 
                                             WA_Width, arg_size_x, 
                                             WA_InnerHeight, arg_size_y,
                                             WA_PubScreen, (ULONG)APP_pubscreen,     
                                             WA_Title, (ULONG)"...",       
                                             WA_Flags, WFLG_DRAGBAR,                                             
                                             TAG_END);
     }

	if (!APP_window) 	
     {
          // Release pens if used.
          if (arg_mode == MODE_BOX)
          {
               if (strlen(arg_box_online_color) > 3)   ReleasePen(APP_pubscreen->ViewPort.ColorMap, arg_box_online_pen);
               if (strlen(arg_box_offline_color) > 3)  ReleasePen(APP_pubscreen->ViewPort.ColorMap, arg_box_offline_pen);
          }

          UnlockPubScreen(NULL, APP_pubscreen);
          return 0;
     }
	else
     {
          // Set Font - needed for Text() funciton.
          SetFont(APP_window->RPort, APP_pubscreen->RastPort.Font);
          return 1;
     }

}
void Intuition_Window_Cleanup(void)
{
     CloseWindow(APP_window);

     // Release pens if used.
     if (arg_mode == MODE_BOX)
     {
          if (strlen(arg_box_online_color) > 3)   ReleasePen(APP_pubscreen->ViewPort.ColorMap, arg_box_online_pen);
          if (strlen(arg_box_offline_color) > 3)  ReleasePen(APP_pubscreen->ViewPort.ColorMap, arg_box_offline_pen);
     }

     UnlockPubScreen(NULL, APP_pubscreen);
}

BYTE Test_Connection_Socket(LONG _ip_converted, BYTE *_ip_status)
{
     // Try open a socket.
	LONG my_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (my_socket == -1) 
          return 0;

	// Try set socket to non-blocking mode.
	LONG mode = 1;
	LONG result = IoctlSocket(my_socket, FIONBIO, &mode);
	if (result == -1) 
     {
          CloseSocket(my_socket);
          return 0;
     }

     // Create primary IP adress structure.
	struct sockaddr_in ip_addr;
	memset ( &ip_addr , 0 , sizeof(struct sockaddr_in) );

	ip_addr.sin_family = AF_INET;
	ip_addr.sin_addr.s_addr = _ip_converted;
	ip_addr.sin_port = htons(80);

     // Try to connect to primary IP.
     connect(my_socket, (struct sockaddr*)&ip_addr, sizeof(ip_addr));
   
     struct timeval timeout;
     timeout.tv_sec = arg_tcp_timeout;
     timeout.tv_usec = 0;
 
     // initialize the bit sets
     fd_set reading, writing, except;

     FD_ZERO( &reading );
     FD_ZERO( &writing );
     FD_ZERO( &except );
 
     // add r, w, and e to the appropriate bit set
     FD_SET( my_socket, &reading );
     FD_SET( my_socket, &writing );
     FD_SET( my_socket, &except );
 
     // for efficiency, what's the maximum socket number? 
     LONG max_sock = 1;

     // poll
     LONG rc = WaitSelect( max_sock, &reading, &writing, &except, &timeout, NULL );

     if (rc > 0)
     {
          // The connection with IP succeded.
          *_ip_status = IP_STATUS_CONNECTED;
          CloseSocket(my_socket);
          return 1;
     }
     else
     {
          // IP connection failed.
          *_ip_status = IP_STATUS_FAILED;
          CloseSocket(my_socket);   
          return 0;
     }
}
BYTE Test_Connection(void)
{
     // Try open socket library.
     SocketBase = (struct Library*)OpenLibrary("bsdsocket.library", APP_BSDSOCKET_LIB_VERSION);
	if (SocketBase == NULL) 
          return 0;

     // Connection status.
     APP_primary_ip_status = IP_STATUS_NOT_USED;
     APP_secondary_ip_status = IP_STATUS_NOT_USED;

     // Trying primary IP.
     APP_primary_ip_converted = inet_addr(arg_primary_ip);
     if (APP_primary_ip_converted == INADDR_NONE) 
     {
          APP_primary_ip_converted = inet_addr(DEF_PRIMARY_IP);
          strcpy(arg_primary_ip, DEF_PRIMARY_IP);
     }

     if (Test_Connection_Socket(APP_primary_ip_converted, &APP_primary_ip_status))
     {
          CloseLibrary(SocketBase);
          return 1;
     }
     else
     {
          // Primary IP failed - testing secondary IP.
          APP_secondary_ip_converted = inet_addr(arg_secondary_ip);
          if (APP_secondary_ip_converted == INADDR_NONE)
          {
               APP_secondary_ip_converted = inet_addr(DEF_SECONDARY_IP);
               strcpy(arg_secondary_ip, DEF_SECONDARY_IP);
          }

          if (Test_Connection_Socket(APP_secondary_ip_converted, &APP_secondary_ip_status))
          {
               CloseLibrary(SocketBase);
               return 1;
          }
          else
          {
               CloseLibrary(SocketBase);
               return 0;               
          }
     }
}

void Cleanup()
{
     // Delete global ENV variable from system.
     DeleteVar(APP_ENV_NAME, GVF_GLOBAL_ONLY);

     if (APP_window_visible) Intuition_Window_Cleanup();

     if (cx_broker) DeleteCxObj(cx_broker);
     if (cx_broker_message_port) DeletePort(cx_broker_message_port);

     // Cleanup Tooltypes.
     ArgArrayDone();

     if (IconBase) CloseLibrary(IconBase);
     if (CxBase) CloseLibrary(CxBase);

     Timer_Cleanup();
}

void Debug_Print(char* _status)
{
     APP_debug_count++;

     char primary_string[16], secondary_string[16];
     
     memset(primary_string, 0, sizeof(primary_string));
     memset(secondary_string, 0, sizeof(secondary_string));

     switch(APP_primary_ip_status)
     {
          case 0:
               strcpy(primary_string, "FAILED");
               break;

          case 1:
               strcpy(primary_string, "CONNECTED");
               break;
          
          default:
               strcpy(primary_string, "NOT USED");
               break;
     }

     switch(APP_secondary_ip_status)
     {
          case 0:
               strcpy(secondary_string, "FAILED");
               break;

          case 1:
               strcpy(secondary_string, "CONNECTED");
               break;
          
          default:
               strcpy(secondary_string, "NOT USED");
               break;
     }

     printf("--- #%d ---\n", APP_debug_count);
     printf("STATUS: %s\n", _status);
     printf("PRIMARY IP: %s (%s)\n", arg_primary_ip, primary_string);
     printf("SECONDARY IP: %s (%s)\n", arg_secondary_ip, secondary_string);
     printf("TIME INTERVAL: %d seconds\n", arg_time_interval);
     printf("TCP TIMEOUT: %d seconds\n", arg_tcp_timeout);
}

// -------------------
// --- Entry point ---
// -------------------
int main(int argc, char **argv)
{
     // -----------------------------
	// --- Init objects and libs ---
     // -----------------------------

	if (!Timer_Init())
	{
		printf("%s: Error! Can't create the timer.", APP_NAME);
		Timer_Cleanup();
		return 1;
	}

     // Lets open all needed libraries.
     CxBase = OpenLibrary((CONST_STRPTR)"commodities.library", 37L);
     if (!CxBase)
     {
          printf("%s: Error! Can't open commodities.library.", APP_NAME);
          Cleanup();
          return 1;
     }

     IconBase = OpenLibrary("icon.library", 37L);
     if (!IconBase)
     {
          printf("%s: Error! Can't open icon.library.", APP_NAME);
          Cleanup();
          return 1;
     }

     // ----------------------
     // --- Init variables ---
     // ----------------------

     APP_window_visible = 0;
     APP_debug_count = 0;

     // Commodities talks to a Commodities application through
     // an Exec Message port, which the application provides
     if ( !(cx_broker_message_port = CreateMsgPort()) )
     {
          printf("%s: Error! Can't create broker message port.", APP_NAME);
          Cleanup();
          return 1;
     }

     cx_newbroker.nb_Port = cx_broker_message_port;

     // Get TOOLTYPES from Icon.

     // Get input arguments stored as TOOLTYPES in program .icon file.
     CONST_STRPTR *tool_types_strings = (CONST_STRPTR*)ArgArrayInit(argc, (CONST_STRPTR*)argv);

     // Get and assign the CX_PRIORITY - if avaiable (standard tooltype for commodities) - if set to 0.
     cx_newbroker.nb_Pri = (char)ArgInt(tool_types_strings, "CX_PRIORITY", 0);

     // Get and assign the CX_POPUP. If YES strart with opened window.
     STRPTR tmp__cx_popup = (STRPTR)ArgString(tool_types_strings, "CX_POPUP", DEF_CX_POPUP);
     if (strcmp(tmp__cx_popup, "YES") == 0)   arg_cx_popup = 1;
     else                                     arg_cx_popup = 0;

     // Get primary and secondary IP.
     arg_primary_ip = (STRPTR)ArgString(tool_types_strings, "PRIMARY_IP", DEF_PRIMARY_IP);
     arg_secondary_ip = (STRPTR)ArgString(tool_types_strings, "SECONDARY_IP", DEF_SECONDARY_IP);

     // Get and validate TIME_INTERVAL
     arg_time_interval = ArgInt(tool_types_strings, "TIME_INTERVAL", DEF_TIME_INTERVAL);
     if (arg_time_interval < 2)    arg_time_interval = DEF_TIME_INTERVAL;
     if (arg_time_interval > 3600) arg_time_interval = 3600;

     // Get and validate TCP_TIMEOUT
     arg_tcp_timeout = ArgInt(tool_types_strings, "TCP_TIMEOUT", DEF_TCP_TIMEOUT);
     if (arg_tcp_timeout < 1) arg_tcp_timeout = DEF_TCP_TIMEOUT;
     if (arg_tcp_timeout > 5) arg_tcp_timeout = 5;

     // Get MODE string and conert to number for easy use.
     STRPTR tmp__mode = (STRPTR)ArgString(tool_types_strings, "MODE", DEF_MODE);
     if (strcmp(tmp__mode, "LABEL") == 0) arg_mode = MODE_LABEL;
     else if (strcmp(tmp__mode, "BOX") == 0) arg_mode = MODE_BOX;
     else arg_mode = MODE_WINDOW_BAR;

     // Get strings for online and offline status.
     arg_online_txt = (STRPTR)ArgString(tool_types_strings, "ONLINE_TXT", DEF_ONLINE_TXT);
     arg_offline_txt = (STRPTR)ArgString(tool_types_strings, "OFFLINE_TXT", DEF_OFFLINE_TXT);

     // Alos save max status string lenght
     if (strlen(arg_online_txt) > strlen(arg_offline_txt))  APP_status_longest_strlen = strlen(arg_online_txt);          
     else                                                   APP_status_longest_strlen = strlen(arg_offline_txt);

     // Get online and offline colors for box (pen number or rgb values).
     // We will validate them during window creation.
     arg_box_online_color = ArgString(tool_types_strings, "BOX_ONLINE_COLOR", DEF_BOX_ONLINE_COLOR);
     arg_box_offline_color = ArgString(tool_types_strings, "BOX_OFFLINE_COLOR", DEF_BOX_OFFLINE_COLOR);

     // Get position and size.
     arg_pos_x = ArgInt(tool_types_strings, "POS_X", DEF_POS_X);
     arg_pos_y = ArgInt(tool_types_strings, "POS_Y", DEF_POS_Y);

     arg_size_x = ArgInt(tool_types_strings, "SIZE_X", DEF_SIZE_X);
     arg_size_y = ArgInt(tool_types_strings, "SIZE_Y", DEF_SIZE_Y);
    
     // Get debug status.
     arg_debug = ArgInt(tool_types_strings, "DEBUG", DEF_DEBUG);

     // Creating the Commodity broker.

     // The commodities.library function CxBroker() adds a broker to the master list.  It takes two arguments,
     // a pointer to a NewBroker structure and a pointer to a LONG.  The NewBroker structure contains information
     // to set up the broker.  If the second argument is not NULL, CxBroker will fill it in with an error code.             
     if ( !(cx_broker = CxBroker(&cx_newbroker, NULL)) )
     {
          // If one instance of the program already is running, this broker won't be created
          // and the application will be closed here.
          // The instance that is already running will be notified about it - and will close itself.
          // So when user will click on icon multiple times - the app will be turned on or turned off like switch. 
          Cleanup();
          return 1;
     }

     // After it's set up correctly, the broker has to be activated.
     ActivateCxObj(cx_broker, 1L);

     // If CX_POPUP tooltype is set to YES - create Window at the beginning.
     if (arg_cx_popup)
     {
          if (!Intuition_Window_Create())
          {
               APP_window_visible = 0;
               printf("%s: Error! Can't create the window.", APP_NAME);
          }
          else
               APP_window_visible = 1;
     }
     else 
          APP_window_visible = 0;


     // --------------------------------------
     // --- Enter the main processing loop ---
     // --------------------------------------

     // Set global ENV variable to "..." at this place.
     SetVar(APP_ENV_NAME, "...", -1, GVF_GLOBAL_ONLY);

     // Commodoty status (enabled/disabled).
     BYTE cx_enabled = 1;

     // The main processing loop status.
     BYTE cx_loop = 1;

     // Send first, short interval to timer - 0 to get first result fast.
     Timer_Send(0, 1);

     while(cx_loop)
     {
          ULONG win_signal   = 1L << APP_window->UserPort->mp_SigBit;
	     ULONG timer_signal = 1L << timer_io->tr_node.io_Message.mn_ReplyPort->mp_SigBit;
	     ULONG cx_signal    = 1L << cx_broker_message_port->mp_SigBit;

          // Wait until any signal appear.
          ULONG signals_received = Wait(win_signal | timer_signal | cx_signal | SIGBREAKF_CTRL_C);          

          // ------------------------------
          // --- Ctrl+C breaking signal ---
          // ------------------------------
          if (signals_received & SIGBREAKF_CTRL_C) 
          {
               cx_enabled = 0;
               cx_loop = 0;
          }

          // -----------------------------------------------------------------
          // --- If signal from commodity, enter commodity processing loop ---
          // -----------------------------------------------------------------
          if ( signals_received & cx_signal )        
          {         
               CxMsg *cx_message;
               ULONG cx_message_id;
               ULONG cx_message_type;     

               while(cx_message = (CxMsg*)GetMsg(cx_broker_message_port))
               {
                    // Extract necessary information from the CxMessage and return it
                    cx_message_id 	= CxMsgID(cx_message);
                    cx_message_type = CxMsgType(cx_message);

                    ReplyMsg((struct Message*)cx_message);

                    switch(cx_message_type)
                    {
                         // Commodities has sent a command
                         case CXM_COMMAND:
                              switch(cx_message_id)
                              {
                                        // User tries to run another instance of program.
                                        // If so - we are turning this instance off.
                                        case CXCMD_UNIQUE:
                                             cx_loop = 0;
                                             cx_enabled = 0;
                                             break;

                                        // User is switching to ACTIVE.
                                        case CXCMD_ENABLE:        
                                             // Try to show window if the CX_POPUP was YES.
                                             if (!APP_window_visible && arg_cx_popup)
                                             {
                                                  if (!Intuition_Window_Create())
                                                  {
                                                       printf("%s: Error! Can't create the window.", APP_NAME);
                                                       APP_window_visible = 0;
                                                  }
                                                  else
                                                       APP_window_visible = 1;
                                             }

                                             // Send short interval for fast result.
                                             Timer_Send(0, 1);

                                             ActivateCxObj(cx_broker, 1L); 
                                             cx_enabled = 1;                                             
                                             break;

                                         // User is switching to INACTIVE.
                                        case CXCMD_DISABLE:
                                             AbortIO(timer_io); 
                                             WaitIO(timer_io);

                                             // If the window is visible - close it.
                                             if (APP_window_visible) Intuition_Window_Cleanup();
                                             APP_window_visible = 0;

                                             // Remove global ENV variable from system when disabling commodity.
                                             DeleteVar(APP_ENV_NAME, GVF_GLOBAL_ONLY);

                                             ActivateCxObj(cx_broker, 0L);
                                             cx_enabled = 0;
                                             break;

                                        // User clicks - SHOW INTERFACE
                                        case CXCMD_APPEAR:                                        
                                             // Try to show window only if the window is not visible.
                                             if (!APP_window_visible && cx_enabled)
                                             {
                                                  if (!Intuition_Window_Create())
                                                  {
                                                       printf("%s: Error! Can't create the window.", APP_NAME);
                                                       APP_window_visible = 0;
                                                  }
                                                  else
                                                       APP_window_visible = 1;
                                             }
                                             break;

                                        // User click - HIDE INTERFACE
                                        case CXCMD_DISAPPEAR:
                                             // Close window only if visible.
                                             if (APP_window_visible) 
                                             {
                                                  Intuition_Window_Cleanup();
                                                  APP_window_visible = 0;
                                             }
                                             break;

                                        // User clicks - REMOVE
                                        case CXCMD_KILL:
                                             cx_loop = 0;
                                             cx_enabled = 0;
                                             break;
                              }
                              break;		
                    }
               }
          }

          // --------------------------------------------------------------------
          // --- If we get the signal from the timer and commodity is enabled, 
          // --- execute our checking routine. 
          // --------------------------------------------------------------------
          if ( (signals_received & timer_signal) && cx_enabled)
          {
               // Using WaitIO() to handle request instead of GetMsg(). 
               WaitIO(timer_io);

               if (Test_Connection())
               {
                    // Set global ENV variable in System to ONLINE.
                    SetVar(APP_ENV_NAME, arg_online_txt, -1, GVF_GLOBAL_ONLY);
                   
                    // Only if window is visible.
                    if (APP_window_visible)
                    {
                         switch(arg_mode)
                         {
                              case MODE_LABEL:
                                   SetAPen(APP_window->RPort, 2);
                                   RectFill(APP_window->RPort, 0, 0, arg_size_x, arg_size_y);
                                   SetAPen(APP_window->RPort, 1);
                                   SetBPen(APP_window->RPort, 2);
                                   Move(APP_window->RPort, 0, APP_pubscreen->RastPort.TxBaseline);
                                   Text(APP_window->RPort, (CONST_STRPTR)arg_online_txt, strlen(arg_online_txt));
                                   SetWindowTitles(APP_window, arg_online_txt, arg_online_txt);
                                   break;

                              case MODE_BOX:
                                   SetAPen(APP_window->RPort, arg_box_online_pen);
                                   RectFill(APP_window->RPort, 0, 0, arg_size_x, arg_size_y);
                                   SetWindowTitles(APP_window, arg_online_txt, arg_online_txt);
                                   break;

                              case MODE_WINDOW_BAR:
                                   SetWindowTitles(APP_window, arg_online_txt, arg_online_txt);
                                   break;
                         }
                    }
             
                    // If debug mode is on - display info in console.
                    if (arg_debug) Debug_Print(arg_online_txt);
               }
               else
               {
                    // Set global ENV variable in System to OFFLINE.
                    SetVar(APP_ENV_NAME, arg_offline_txt, -1, GVF_GLOBAL_ONLY);

                    // Only if window is visible.
                    if (APP_window_visible)
                    {
                         switch(arg_mode)
                         {
                              case MODE_LABEL:
                                   SetAPen(APP_window->RPort, 2);
                                   RectFill(APP_window->RPort, 0, 0, arg_size_x, arg_size_y);
                                   SetAPen(APP_window->RPort, 1);
                                   SetBPen(APP_window->RPort, 2);
                                   Move(APP_window->RPort, 0, APP_pubscreen->RastPort.TxBaseline);
                                   Text(APP_window->RPort, (CONST_STRPTR)arg_offline_txt, strlen(arg_offline_txt));
                                   SetWindowTitles(APP_window, arg_offline_txt, arg_offline_txt);
                                   break;

                              case MODE_BOX:
                                   SetAPen(APP_window->RPort, arg_box_offline_pen);
                                   RectFill(APP_window->RPort, 0, 0, arg_size_x, arg_size_y);
                                   SetWindowTitles(APP_window, arg_offline_txt, arg_offline_txt);
                                   break;

                              case MODE_WINDOW_BAR:
                                   SetWindowTitles(APP_window, arg_offline_txt, arg_offline_txt);
                                   break;
                         }
                    }

                    // If debug mode is on - display info in console.
                    if (arg_debug) Debug_Print(arg_offline_txt);
               }

               Timer_Send(arg_time_interval, 0);
          }                           

          // ------------------------------------------------------------------------
          // --- If signal from window (if visible), enter window processing loop ---
          // ------------------------------------------------------------------------
	     if ( ( signals_received & win_signal) && cx_enabled && APP_window_visible)
		{
               struct IntuiMessage *imsg;

               while( (imsg = GT_GetIMsg(APP_window->UserPort)) )
               {
                    GT_ReplyIMsg(imsg);                 
               }
		}        
     }

     // Clean up.
     Cleanup();

	return 0;
}
