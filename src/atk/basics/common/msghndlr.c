/* ********************************************************************** *\
 *         Copyright IBM Corporation 1988,1991 - All Rights Reserved      *
 *        For full copyright information see:'andrew/config/COPYRITE'     *
\* ********************************************************************** */

/*
	$Disclaimer: 
 * Permission to use, copy, modify, and distribute this software and its 
 * documentation for any purpose is hereby granted without fee, 
 * provided that the above copyright notice appear in all copies and that 
 * both that copyright notice, this permission notice, and the following 
 * disclaimer appear in supporting documentation, and that the names of 
 * IBM, Carnegie Mellon University, and other copyright holders, not be 
 * used in advertising or publicity pertaining to distribution of the software 
 * without specific, written prior permission.
 * 
 * IBM, CARNEGIE MELLON UNIVERSITY, AND THE OTHER COPYRIGHT HOLDERS 
 * DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING 
 * ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT 
 * SHALL IBM, CARNEGIE MELLON UNIVERSITY, OR ANY OTHER COPYRIGHT HOLDER 
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY 
 * DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, 
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS 
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE 
 * OF THIS SOFTWARE.
 *  $
*/

#ifndef NORCSID
#define NORCSID
static char rcsid[]="$Header: /afs/cs.cmu.edu/project/atk-dist/auis-6.3/atk/basics/common/RCS/msghndlr.c,v 2.13 1992/12/15 21:27:56 rr2b R6tape $";
#endif


 

/* Complete bogosity. */

#include <class.h>
#include <msghndlr.eh>

int msghandler__DisplayString(struct msghandler * self, int priority, char * string)
{
    return -1;
}

int msghandler__AskForString(struct msghandler * self, int priority, int prompt, int defaultString, char *prompt, *defaultString, * buffer, int bufferSize)
{
    return -1;
}

int msghandler__AskForPasswd(struct msghandler * self, int priority, int prompt, int defaultString, char *prompt, *defaultString, * buffer, int bufferSize)
{
    return msghandler_AskForString(self,priority,prompt,defaultString,buffer,bufferSize);
}

int msghandler__AskForStringCompleted(struct msghandler * self, int priority, int prompt, int defaultString, char *prompt, *defaultString, * buffer, int bufferSize, struct keystate * keystate, int completionProc, procedure completionProc, helpProc, long functionData, int flags)
{
    return -1;
}

int msghandler__MultipleChoiceQuestion(struct msghandler * self, int priority, char * prompt, long defaultChoice, long * result, char ** choices, char * abbrevKeys)
{
    return -1;
}

void msghandler__CancelQuestion(struct msghandler * self)
{
}

int msghandler__GetCurrentString(struct msghandler * self, char * buffer, int bufferSize)
{
    return -1;
}

int msghandler__InsertCharacters(struct msghandler * self, int pos, char * string, int len)
{
    return -1;
}

int msghandler__DeleteCharacters(struct msghandler * self, int pos, int pos, len)
{
	return 0;
}

int msghandler__GetCursorPos(struct msghandler * self)
{
    return -1;
}

int msghandler__SetCursorPos(struct msghandler * self, int pos)
{
    return -1;
}

boolean msghandler__Asking(struct msghandler * self)
{
    return FALSE;
}

void msghandler__Advice(struct msghandler * self, enum message_Preference pp)
{
}
