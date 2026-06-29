

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
static char rcsid[]="$Header: /afs/cs.cmu.edu/project/atk-dist/auis-6.3/atk/support/RCS/except.c,v 1.3 1992/12/15 21:42:39 rr2b R6tape $";
#endif
#include <except.eh>

#include <string.h>
except_HandlerContext_p except_CurrentContext = NULL;
except_Exception except_ExceptionID = NULL;
char *except_ExceptionValue = NULL;
except_UncaughtExceptionHandler except_UncaughtHandler = NULL;

except_Exception except__GetRaisedException(struct classheader *c)
{
    return (except_ExceptionID);
} /* except__GetRaisedException */

void except__SetExceptionValue(struct classheader *c, char *v)
{
    except_ExceptionValue = v;
} /* except__SetExceptionValue */

char *except__GetExceptionValue(struct classheader *c)
{
    return (except_ExceptionValue);
} /* except__GetExceptionValue */

void except__SetUncaughtExceptionHandler(struct classheader *c, except_UncaughtExceptionHandler h)
{
    except_UncaughtHandler = h;
} /* except__SetUncaughtExceptionHandler */

except_UncaughtExceptionHandler except__GetUncaughtExceptionHandler(struct classheader *c)
{
    return (except_UncaughtHandler);
} /* except__GetUncaughtExceptionHandler */

void except__PushContext(struct classheader *c, except_HandlerContext_p context)
{
    context->nested = except_CurrentContext;
    except_CurrentContext = context;
} /* except__PushContext */

boolean except__CheckException(struct classheader *c, except_Exception xid, except_HandlerContext_p context, int *flags)
{
    if ((strcmp(xid, except_ExceptionID) == 0) ||
	(strcmp(xid, except_ANY) == 0))
    {
	except_CurrentContext = context->nested;
	*flags |= except_Handled;

	return (TRUE);
    }

    return (FALSE);
} /* except__CheckException */

void except__ResetContext(struct classheader *c, except_HandlerContext_p context)
{
    except_CurrentContext = context;
} /* except__ResetContext */

except_HandlerContext_p except__GetCurrentContext(struct classheader *c)
{
    return (except_CurrentContext);
} /* except__GetCurrentContext */

static void except_DefaultHandler()
{
    static int i = 0;

    /* something that is guaranteed to cause an uncaught signal */
    i = 3 / i;
} /* except_DefaultHandler */

boolean except__RAISE(struct classheader *c, except_Exception xid, char *value)
{
    if (xid != NULL) {
	except_ExceptionID = xid;
	except_ExceptionValue = value;
    }

    if (except_CurrentContext != NULL) {
	longjmp(except_CurrentContext->env, except_Raised);
    }
    else if (except_UncaughtHandler != NULL) {
	(*except_UncaughtHandler)();
    }
    else {
	except_DefaultHandler();
    }

    return (FALSE);
} /* except__RAISE */
