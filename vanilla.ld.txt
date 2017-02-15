/*. Entry Point *./
ENTRY( Reset_Handler )

/* Highest address of the user mode stack */
_estack = 0x20010000; /* end o f 64K RAM */

/*. Generate a link error if heap and s tack don’t fit int o RAM .*/
_Min_Heap_Size = 0; /* required amount of heap */
_Min_Stack_Size = 0x200; /*. required amount of stack .*/

/*. Specify the memory areas .*/
MEMORY
{
     FLASH ( rx ) : ORIGIN = 0x08000000 , LENGTH = 256K
     RAM ( xrw) : ORIGIN = 0x20000000 , LENGTH = 64K
     MEMORY_B1 ( rx ) : ORIGIN = 0x60000000 , LENGTH = 0K
}
