/* ********************************************************************** *\
 *         Copyright IBM Corporation 1988,1991 - All Rights Reserved      *
 *        For full copyright information see:'andrew/config/COPYRITE'     *
\* ********************************************************************** */
static char *getsubs_rcsid = "$Header: /afs/cs.cmu.edu/project/atk-dist/auis-6.3/ams/libs/ms/RCS/getsubs.c,v 2.3 1991/09/12 15:44:21 bobg R6tape $";

/*
$Header: /afs/cs.cmu.edu/project/atk-dist/auis-6.3/ams/libs/ms/RCS/getsubs.c,v 2.3 1991/09/12 15:44:21 bobg R6tape $
$Source: /afs/cs.cmu.edu/project/atk-dist/auis-6.3/ams/libs/ms/RCS/getsubs.c,v $
*/
extern int GetSubsEntry();

int MS_GetSubscriptionEntry(char *FullName, char *NickName, int *status)
{
    return(GetSubsEntry(FullName, NickName, status));
}

