
//Common
#include <common/types.h>
#include <common/printf.h>
#include <common/timer.h>

//Hardware com
#include <hardwarecommunication/interrupts.h>
#include <hardwarecommunication/pci.h>
#include <hardwarecommunication/serial.h>

//Drivers
#include <drivers/driver.h>
#include <drivers/keyboard.h>
#include <drivers/mouse.h>
#include <drivers/vga.h>
#include <drivers/ata.h>
#include <drivers/amd_am79c973.h>

//GUI
#include <gui/desktop.h>
#include <gui/window.h>
#include <gui/render.h>
#include <gui/widgets/text.h>

//NET
#include <net/etherframe.h>
#include <net/arp.h>
#include <net/ipv4.h>
#include <net/icmp.h>
#include <net/udp.h>

//SYSTEM
#include <system/process.h>
#include <system/gdt.h>
#include <system/syscalls.h>
#include <system/memorymanagement.h>
#include <system/multithreading.h>

using namespace maxOS;
using namespace maxOS::common;
using namespace maxOS::drivers;
using namespace maxOS::hardwarecommunication;
using namespace maxOS::gui;
using namespace maxOS::net;
using namespace maxOS::system;


// #define ENABLE_GRAPHICS

static Console console;

/**
 * @details Compare two strings
 * @param  s1 String 1
 * @param  s2 String 2
 * @return 0 if equal, 1 if not
 */
int strcmp(const char* s1, const char* s2)
{
    while ((*s1 == *s2) && *s1) { ++s1; ++s2; }                             //While the characters are the same and the string is not over
    return ((int) (unsigned char) *s1) - ((int) (unsigned char) *s2);       //Return the difference between the characters
}


/**
 * @details Get the length of a string
 * @param  str String to print
 * @param lines Lines to print on
 */
void printf_gui(char* str, Text lines[15]){
    console.put_string_gui(str,lines);                                      //Print to the GUI console


}

/**
 * @details Print to the Video Memory Debug Console
 * @param  str String to print
 */

bool testTick = false;
void printf(char* str, bool clearLine = false)
{

    console.put_string(str,clearLine);

}
/**
 * @details Print to the Video Memory Debug Console
 * @param  key Hex Value to print
 */
void printfHex(uint8_t key){
    console.put_hex(key);
}



char printfInt( int i)
{
    printf(console.int_to_string(i));

}



class PrintfKeyboardEventHandler : public KeyboardEventHandler{
    public:

        void OnKeyDown(char* c){

           if(strcmp(c,"ARRT") == 0){

                console.moveCursor(1,0);
                return;

            }else if(strcmp(c,"ARLF") == 0){

                console.moveCursor(-1,0);
                return;

            }else if(strcmp(c,"ARDN") == 0){

                console.moveCursor(0,1);
                return;

            }else if(strcmp(c,"ARUP") == 0){

                console.moveCursor(0,-1);
                return;

            }else if(strcmp(c,"BACKSPACE") == 0){

                console.backspace();

           }else if(strcmp(c,"t") == 0){

               printf("T");
                    testTick = true;


           }else{

                printf(c);                          //Print Char
                printf("_");                        //Line to know where cursor at
                console.moveCursor(-1,0);      //Move behind the cursor

            }





        }
};

class MouseToConsole: public MouseEventHandler{

        int8_t x, y;
    public:
        MouseToConsole()
        {
            static uint16_t* VideoMemory = (uint16_t*)0xb8000;
            x = 40;
            y = 12;
            //Show the initial cursor
            VideoMemory[80*y+x] = (VideoMemory[80*y+x] & 0x0F00) << 4           //Get High 4 bits and shift to right (Foreground becomes Background)
                                  | (VideoMemory[80*y+x] & 0xF000) >> 4         //Get Low 4 bits and shift to left (Background becomes Foreground)
                                  | (VideoMemory[80*y+x] & 0x00FF);             //Keep the last 8 bytes the same (The character)
        }

        void OnMouseMove(int x_offset, int y_offset){

            static uint16_t* VideoMemory = (uint16_t*)0xb8000;

            //Show old cursor
            VideoMemory[80*y+x] = (VideoMemory[80*y+x] & 0x0F00) << 4           //Get High 4 bits and shift to right (Foreground becomes Background)
                                  | (VideoMemory[80*y+x] & 0xF000) >> 4         //Get Low 4 bits and shift to left (Background becomes Foreground)
                                  | (VideoMemory[80*y+x] & 0x00FF);             //Keep the last 8 bytes the same (The character)

            x += x_offset;     //Movement on the x-axis
            y += y_offset;     //Movement on the y-axis (note, mouse passes the info inverted)

            //Make sure mouse position not out of bounds
            if(x < 0) x = 0;
            if(x >= 80) x = 79;

            if(y < 0) y = 0;
            if(y >= 25) y = 24;



            //Show the new cursor by inverting the current character
            VideoMemory[80*y+x] = (VideoMemory[80*y+x] & 0x0F00) << 4           //Get High 4 bits and shift to right (Foreground becomes Background)
                                  | (VideoMemory[80*y+x] & 0xF000) >> 4         //Get Low 4 bits and shift to left (Background becomes Foreground)
                                  | (VideoMemory[80*y+x] & 0x00FF);             //Keep the last 8 bytes the same (The character)
        }

};

class PrintfUDPHandler : public UserDatagramProtocolHandler
{
public:
    void HandleUserDatagramProtocolMessage(UserDatagramProtocolSocket* socket, common::uint8_t* data, common::uint16_t size)
    {
        char* foo = " ";
        for(int i = 0; i < size; i++)                                    //Loop through the data
        {
            foo[0] = data[i];                                                   //Get the character
            printf(foo);                                                        //Print the character
        }
    }
};


/**
 * @details Print a string via a syscall
 * @param str String to print
 */



void sys_printf(char* str)
{


    //asm("int $0x80" : : "a" (4), "b" (str));        //Call the interrupt 0x80 with the syscall number 4 and the string to print
    asm volatile( "int $0x80" : "=c"(str) : "a"(4), "b"(str));

}

void sys_getpid(uint32_t* funct)
{
    asm("int $0x80" : : "a" (20), "b" (funct));        //Call the interrupt 0x80 with the syscall number 20 and the process to get PID from


}


void sys_kill(int pid)
{
    asm("int $0x80" : : "a" (37), "b" (pid));        //Call the interrupt 0x80 with the syscall number 21 and the process to kill
}

void temp_sys_kill()
{
    asm("int $0x20" : : "a" (37), "b" (1));        //Call the interrupt 0x80 with the syscall number 20 and the process to kill (1 is is kill me)
}

void proc_exit(char* status)
{
    sys_printf("\nProcess Exited with status: ");
    sys_printf(status);
    sys_printf("\n");
    temp_sys_kill();

}

void taskA()
{
    for (int i = 0; i < 100; ++i) {

        sys_printf("A");
    }

    Timer::activeTimer ->Wait(100);

    for (int i = 0; i < 100; ++i) {

        sys_printf("B");
    }

    proc_exit("0");



}



void taskB()
{
    while(true)
        sys_printf("B");
}



//Define what a constructor is
typedef void (*constructor)();

//Iterates over space between start_ctors and end_ctors and jumps into all function pointers
extern "C" constructor start_ctors;
extern "C" constructor end_ctors;
extern "C" void callConstructors()
{
    for(constructor* i = &start_ctors; i != &end_ctors; i++)        //Iterate over all constructors
        (*i)();                                                     //Call the constructor
}


class Version{
    public:
       int version;            //Set based on a noticeable feature update eg. Keyboard Driver etc. [not] code comment updates etc.
       int build;              //Commit Number
       char* buildAuthor;      //Author of the commit (Mostlikly me, but change for pull requests and such)
        Version(){};
        ~Version(){};

};

//Network
uint8_t ip[] = {10,0,2,15};              //IP address that vms have for virtualization
uint32_t IP_BE = ((uint32_t)ip[3] << 24)                //Convert to big endian
                 | ((uint32_t)ip[2] << 16)
                 | ((uint32_t)ip[1] << 8)
                 | (uint32_t)ip[0];

uint8_t gateway_ip[] = {10,0,2,2};       //IP address for the gateway
uint32_t GIP_BE = ((uint32_t)gateway_ip[3] << 24)       //Convert to big endian
                  | ((uint32_t)gateway_ip[2] << 16)
                  | ((uint32_t)gateway_ip[1] << 8)
                  | (uint32_t)gateway_ip[0];

uint8_t subnet[] = {255,255,255,0};      //Subnet mask
uint32_t SUB_BE = ((uint32_t)subnet[3] << 24)           //Convert to big endian
                  | ((uint32_t)subnet[2] << 16)
                  | ((uint32_t)subnet[1] << 8)
                  | (uint32_t)subnet[0];

///__KERNEL PROCESS VARS__

serial k_sLog = 0;
InternetProtocolProvider *k_ipv4;

/**
 * @details Main process for the kernel
 * @return Should never return, nor be killed
 */
void kernProc(){
    printf("\n[x] Kernel Process Started\n");
    /*Note: Serial port sends random interrupt before interrupts are activated*/     k_sLog.Write("Devices Ready\n",1); k_sLog.Write("Network Ready\n",1);

    k_sLog.Write("Interrupts Ready\n",1);

    //Kernel is ready, code after here should be in a separate process
    k_sLog.Write("MaxOS is ready\n",7);



    while(1){
        #ifdef ENABLE_GRAPHICS
                            //render new frame
                            desktop.Draw(&rend);

                            //display rendered frame
                             rend.display(&vga);
        #endif
    }

}

#pragma clang diagnostic ignored "-Wwritable-strings"

extern "C" void kernelMain(const void* multiboot_structure, uint32_t multiboot_magic)
{


    //NOTE: Will rewrite boot text stuff later

    Version* maxOSVer;
    maxOSVer->version = 25;
    maxOSVer->build = 62;
    maxOSVer->buildAuthor = "Max Tyson";

    //Print in header
    console.lim_x = 80;
    console.x = 30; console.y = 1;
    printf("* Max OS Kernel -v0."); printfInt(maxOSVer->version);    printf(" * -b"); printfInt(maxOSVer->build);  printf(" * -a"); printf(maxOSVer->buildAuthor);

    //Reset Console positions
    console.x = 1; console.y = 3;

    console.ini_x = 2;
    console.ini_y = 4;

    console.lim_x = 79;
    console.lim_y = 24;



    printf("\n[x] Kernel Booted \n");

    printf("[ ] Setting Programmable Interrupt Timer ... \n");
    Timer t;
    PIT pit(10);
    printf("[x] PIT Setup \n");


    printf("[ ] Setting Up Global Descriptor Table... \n");
    GlobalDescriptorTable gdt;                                                              //Setup GDT
    printf("[x] GDT Setup \n");

    printf("[ ] Setting Up Memory Management... \n");
    uint32_t* memupper = (uint32_t*)(((size_t)multiboot_structure) + 8);                    //memupper is a field at offset 8 in the Multiboot information structure and it indicates the amount upper memory in kilobytes.
                                                                                            //Lower memory starts at address 0, and upper memory starts at address 1 megabyte. The
                                                                                            //maximum possible value for lower memory is 640 kilobytes. The value returned for upper
                                                                                            //memory is maximally the address of the first upper memory hole minus 1 megabyte.
    size_t  heap = 10*1024*1024;                                                            //Start at 10MB

    //Print the heap address
    printf("heap: 0x");
    printfHex((heap >> 24) & 0xFF);
    printfHex((heap >> 16) & 0xFF);
    printfHex((heap >> 8 ) & 0xFF);
    printfHex((heap      ) & 0xFF);


    size_t  memSize = (*memupper)*1024 - heap - 10*1024;                                    //Convert memupper into MB, then subtract the hep and some padding
    MemoryManager memoryManager(heap, memSize);                                    //Memory Mangement
    //Print the memory adress
    printf(" memSize: 0x");
    printfHex(((size_t)memSize >> 24) & 0xFF);
    printfHex(((size_t)memSize >> 16) & 0xFF);
    printfHex(((size_t)memSize >> 8 ) & 0xFF);
    printfHex(((size_t)memSize      ) & 0xFF);


    void* allocated = memoryManager.malloc(1024);
    printf(" allocated: 0x");
    printfHex(((size_t)allocated >> 24) & 0xFF);
    printfHex(((size_t)allocated >> 16) & 0xFF);
    printfHex(((size_t)allocated >> 8 ) & 0xFF);
    printfHex(((size_t)allocated      ) & 0xFF);
    printf("\n");

    printf("[x] Memory Management Setup \n");


    printf("[ ] Setting Thread Manager... \n");
    ThreadManager threadManager;

    /*
    __Tests__
    Task task1(&gdt, taskA);
    Task task2(&gdt, taskB);
    taskManager.AddTask(&task1);
    taskManager.AddTask(&task2);

     */




    printf("[x] Task Manager Setup \n");




    printf("[ ] Setting Up Interrupt Manager... \n");
    InterruptManager interrupts(0x20, &gdt, &threadManager);            //Instantiate the method
    printf("[x] Interrupt Manager Setup \n", true);

    printf("[ ] Setting Up Serial Log... \n");
    serial serialLog(&interrupts);
    //serialLog.Test();
    serialLog.Write("\n",-1);
    serialLog.Write("Serial Log Started\n");
    serialLog.Write("MaxOS Started\n",7);
    printf("[x] Serial Log Setup \n");



    printf("[ ] Setting Up System Calls Handler... \n");
    SyscallHandler syscalls(&interrupts, 0x80);                               //Instantiate the method
    printf("[x] System Calls Handler Setup \n", true);

    serialLog.Write("Memory Management Ready\n",1);
    serialLog.Write("Tasks Ready\n",1);
    serialLog.Write("Global Descriptor Table Ready\n",1);
    serialLog.Write("System Calls Ready\n",1);

    #ifdef ENABLE_GRAPHICS
        //Desktop needs the mouse and keyboard
        // so instantiated it here
        Desktop desktop(320,200,0x00,0x00,0xA8);
    #endif

    printf("[ ] Setting Up Drivers... \n");

    DriverManager driverManager;
        //Keyboard
        #ifdef GRAPHICSMODE
            KeyboardDriver keyboard(&interrupts, &desktop);
        #else
            PrintfKeyboardEventHandler kbhandler;
            KeyboardDriver keyboard(&interrupts, &kbhandler);

        #endif
        driverManager.AddDriver(&keyboard);
        printf("    -Keyboard setup\n");


        //Mouse
        #ifdef GRAPHICSMODE
            MouseDriver mouse(&interrupts, &desktop);
        #else
            MouseToConsole mousehandler;
            MouseDriver mouse(&interrupts, &mousehandler);
        #endif
            driverManager.AddDriver(&mouse);
        printf("    -Mouse setup\n");

        printf("    -[ ]Setting PCI\n\n");
        PeripheralComponentInterconnectController PCIController;
        PCIController.SelectDrivers(&driverManager, &interrupts);
        printf("\n    -[x]Setup PCI\n");

        #ifdef ENABLE_GRAPHICS
            VideoGraphicsArray vga;
            Render rend(320,200);   //arguments do nothing for now. 320,200 is hardcoded into "gui/render.h"
            printf("    -VGA setup\n");
        #endif

    driverManager.ActivateAll();
    printf("[X] Drivers Setup\n");


    #ifdef ENABLE_GRAPHICS
        vga.SetMode(320,200,8);

        //Window
        Window debugConsole(&desktop, 10,10,250,150,0x00,0x00,0x00);
        Text debugConsole_title(&debugConsole,2,3,200,10,0x00,0x00,0x00,"Debug Console");
        debugConsole.AddChild(&debugConsole_title);


        //Debug console lines (note will make into app later)
        Text lines[15] = {
                Text(&debugConsole,2,20,10,10,0xA8,0x00,0x00,""),
                Text(&debugConsole,2,30,10,10,0xA8,0x00,0x00,""),
                Text(&debugConsole,2,40,10,10,0xA8,0x00,0x00,""),
                Text(&debugConsole,2,50,10,10,0xA8,0x00,0x00,""),
                Text(&debugConsole,2,60,10,10,0xA8,0x00,0x00,""),
                Text(&debugConsole,2,70,10,10,0xA8,0x00,0x00,""),
                Text(&debugConsole,2,80,10,10,0xA8,0x00,0x00,""),
                Text(&debugConsole,2,90,10,10,0xA8,0x00,0x00,""),
                Text(&debugConsole,2,90,10,10,0xA8,0x00,0x00,""),
                Text(&debugConsole,2,100,10,10,0xA8,0x00,0x00,""),
                Text(&debugConsole,2,110,10,10,0xA8,0x00,0x00,""),
                Text(&debugConsole,2,120,10,10,0xA8,0x00,0x00,""),
                Text(&debugConsole,2,130,10,10,0xA8,0x00,0x00,""),
                Text(&debugConsole,2,140,10,10,0xA8,0x00,0x00,""),
                Text(&debugConsole,2,150,10,10,0xA8,0x00,0x00,""),

        };

        for (int i = 0; i <  15; ++i) {
            debugConsole.AddChild(&lines[i]);
        }




        //Add children

        desktop.AddChild(&debugConsole);


        printf_gui("GUI is ready ............ \n",lines);
        printf_gui("Max OS\n\n",lines);
        printf_gui("LONG LINE (*(#()*$(@*#($#*@()$*#@",lines);
        //BUG: Long lines
    #endif

    printf("[ ] Setting Up ATA Hard Drives... \n");

    //Interrupt 14 for Primary
    AdvancedTechnologyAttachment ata0m(0x1F0, true);         //Primary master
    AdvancedTechnologyAttachment ata0s(0x1F0, false);        //Primary Slave
    printf("    -ATA Primary Master: ");     ata0m.Identify();     printf("\n");
    printf("    -ATA Primary Slave: ");      ata0s.Identify();     printf("\n");

    //Interrupt 15 for Primary
    AdvancedTechnologyAttachment ata1m(0x170, true);         //Secondary master
    AdvancedTechnologyAttachment ata1s(0x170, false);        //Secondary Slave

    /*

    AdvancedTechnologyAttachment ata1m(0x1E8, true);         //Third master
    AdvancedTechnologyAttachment ata1s(0x1E8, false);        //Third Slave

    AdvancedTechnologyAttachment ata1m(0x168, true);         //Third master
    AdvancedTechnologyAttachment ata1s(0x168, false);        //Third Slave

    */

    //Test

    char* atamBuffer = "Test file write";
    printf("    -ATA Write test: ");
    ata0m.Write28(0,(uint8_t*)atamBuffer,15);
    printf("\n");
    ata0m.Flush();
    printf("    -ATA Read test: ");
    ata0m.Read28(0,15);
    printf("\n");


    printf("[x] ATA Hard Drives Setup \n");



    printf("[x] Setting Up Network Driver \n");
    amd_am79c973* eth0 = (amd_am79c973*)(driverManager.drivers[2]);

        printf(" -  Setting Up IP, Gateway, Subnet... \n");
        eth0 -> SetIPAddress(IP_BE);

        printf(" -  Setting Up EtherFrame... \n");
        EtherFrameProvider etherFrame(eth0);

        printf(" -  Setting Up ARP... \n");
        AddressResolutionProtocol arp(&etherFrame);

        printf(" -  Setting Up IPv4... \n");
        InternetProtocolProvider ipv4(&etherFrame, &arp, GIP_BE, SUB_BE);

        printf(" -  Setting Up ICMP... \n");
        InternetControlMessageProtocol icmp(&ipv4);

        printf(" -  Setting Up UDP... \n");
        UserDatagramProtocolProvider udp(&ipv4);
        PrintfUDPHandler printfUDPHandler;

    printf("[x] Network Driver Setup \n");

    //Process kernelMain(kernProc, &threadManager);
    //Process testProcess(taskA, &threadManager);

    k_sLog = serialLog;
    k_ipv4 = &ipv4;



    //Interrupts should be the last thing as once the clock interrupt is sent the multitasker will start doing processes and tasks
    printf("[ ] Activating Interrupt Descriptor Table... \n");
    interrupts.Activate();
    printf("[x] IDT Activated \n", true);

    printf("++Broadcast MAC ++\n");
    arp.BroadcastMACAddress(GIP_BE);

    printf("\n ++ Test ICMP ++\n");
    icmp.RequestEchoReply(GIP_BE);

    printf("\n ++ Test UDP ++\n");
    /* Test Connection TODO: Comeback and fix the UDP handler
    UserDatagramProtocolSocket* test_socket = udp.Connect(GIP_BE, 1234);        //Use ncat -u -l 1234 to test
    udp.Bind(test_socket, &printfUDPHandler);
    test_socket->Send((uint8_t*)"Hello World", 11);
     */

    //Test Listening
    UserDatagramProtocolSocket* test_socket = udp.Listen(1234);        //First set a portforward rule in Vbox then Use ncat -u -p 1234 127.0.0.1 to test
    udp.Bind(test_socket, &printfUDPHandler)




}




#pragma clang diagnostic pop

//MaxOS - May the source be with you...