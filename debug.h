/*

  (c) A-Vision Software

*/

#ifndef __DEBUG_H__
#define __DEBUG_H__

#ifndef DEBUG
#define DEBUG false
#endif

#if DEBUG

#define DBG(msg, newline) \
Serial.print(msg); \
if (newline) Serial.println();

#else
#define DBG(msg, newline)
#endif

#endif
