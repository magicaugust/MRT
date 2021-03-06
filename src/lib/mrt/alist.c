/*
 * $Id: alist.c,v 1.1.1.1 2000/08/14 18:46:11 labovit Exp $
 */

#include "mrt.h"

static LINKED_LIST *access_list[MAX_ALIST];


static condition_t * 
find_access_list (int num, int permit, prefix_t *prefix, prefix_t *wildcard,
		  int exact, int refine) {
  condition_t *condition;

  if (num < 0 || num >= MAX_ALIST)
    return (NULL);
  if (access_list[num] == NULL) {
    return (NULL);
  }

   LL_Iterate (access_list[num], condition) {
     if (permit != condition->permit)
	continue;
     if (prefix == NULL && condition->prefix == NULL) {
	/* OK, both are all */
     }
     else if (prefix && condition->prefix) {
	if (!prefix_compare (prefix, condition->prefix))
	    continue;
	if (wildcard == NULL && condition->wildcard == NULL) {
	    /* OK */
	}
	else if (wildcard && condition->wildcard) {
	    if (!prefix_compare (wildcard, condition->wildcard))
	        continue;
	}
	else {
	    continue;
	}
        if (exact != condition->exact)
	   continue;
        if (refine != condition->refine)
	   continue;
     }
     else {
	/* one is prefix, the other is all */
	continue;
     }

       return (condition);
   }

   return (NULL);
}


int
add_access_list (int num, int permit, prefix_t *prefix, prefix_t *wildcard, 
		 int exact, int refine) {

   condition_t *condition = NULL;

   if (num < 0 || num >= MAX_ALIST)
	 return (-1);
   if (find_access_list (num, permit, prefix, wildcard, exact, refine))
	return (0);

   if (access_list[num] == NULL) {
      access_list[num] = LL_Create (0);
   }
   condition = New (condition_t);
   condition->permit = permit;
   /* expects Ref_Prefix can handle even a case of prefix == NULL */
   condition->prefix = Ref_Prefix (prefix);
   condition->wildcard = Ref_Prefix (wildcard);
   condition->exact = exact;
   condition->refine = refine;
   LL_Add (access_list[num], condition);
   return (LL_GetCount (access_list[num]));
}


int 
remove_access_list (int num, int permit, prefix_t *prefix, prefix_t *wildcard,
		    int exact, int refine) {
  condition_t *condition;

  if (num < 0 || num >= MAX_ALIST)
    return (-1);
  if (access_list[num] == NULL) {
    return (-1);
  }
  condition = find_access_list (num, permit, prefix, wildcard, exact, refine);
  if (condition == NULL)
    return (-1);

   Deref_Prefix (condition->prefix);
   Deref_Prefix (condition->wildcard);
   LL_Remove (access_list[num], condition);

   return (LL_GetCount (access_list[num]));
}


int
del_access_list (int num) {

   if (num < 0 || num >= MAX_ALIST)
	 return (-1);
    if (access_list[num] == NULL) {
        return (-1);
    }
    Delete (access_list[num]);
    access_list[num] = NULL;
    return (1);
}


/*
 * return 1 if permit, 0 if denied, -1 if not matched
 */
int 
apply_condition (condition_t *condition, prefix_t *prefix)
{
      assert (condition);

      if (condition->prefix == NULL) /* all */
	  return (condition->permit);

	  /* family the same */
      if ((prefix->family == condition->prefix->family) &&
	  /* refine */
	  ((condition->refine && prefix->bitlen > condition->prefix->bitlen) ||
	  /* no refine or exact */
          ((!condition->refine || condition->exact) &&
              prefix->bitlen == condition->prefix->bitlen)) &&
	  /* prefix compare up to bitlen with wildcard */
          byte_compare (prefix_tochar (prefix), 
                        prefix_tochar (condition->prefix),
                        condition->prefix->bitlen, 
			(condition->wildcard)? 
			    prefix_tochar (condition->wildcard): NULL))
          return (condition->permit);
      return (-1);
}


/*
 * return 1 if permit, 0 otherwise
 */
int apply_access_list (int num, prefix_t *prefix) {

   condition_t *condition;

   if (num < 0 || num >= MAX_ALIST)
     return (0);
   if (num == 0)
      return (1); /* cisco feature */
   if (access_list[num] == NULL) {
      /* I'm not sure how cisco works for undefined access lists */
     return (0); /* assuming deny for now */
   }

   LL_Iterate (access_list[num], condition) {
      int value;

      if ((value = apply_condition (condition, prefix)) >= 0)
	  return (value);
   }
   /* deny */
   return (0);
}


/* this will be obsolete */
char *
access_list_toa2 (int num) {
    condition_t *condition;
    char buff[MAXLINE];
    char *start, *cp = NULL;
    int mode;
    int len = 0;
  
    if (access_list[num] == NULL)
        return (NULL);

for (mode = 0; mode <=1 ; mode++) {

    if (mode == 0)
	start = buff;
    else {
	cp = start = NewArray (char, len + 1);
    }

    LL_Iterate (access_list[num], condition) {
        char *tag;

	if (mode == 0)
	    cp = start;

        if (condition->permit) 
	    tag = "permit";
        else
	    tag = "deny";

        if (condition->prefix == NULL) {
            sprintf (cp, "access-list %d %s all\n", num, tag);
	    cp += strlen (cp);
	}
        else {
            sprintf (cp, "access-list %d %s %s", num, tag, 
		     prefix_toax (condition->prefix));
	    cp += strlen (cp);

	    if (condition->wildcard) {
		sprintf (cp, " %s", prefix_toa (condition->wildcard));
	        cp += strlen (cp);
	    }
            if (condition->exact) {
	        strcpy (cp, " exact");
	        cp += strlen (cp);
	    }
            if (condition->refine) {
	        strcpy (cp, " refine");
	        cp += strlen (cp);
	    }
	    strcpy (cp, "\n");
	    cp += strlen (cp);
	}
	if (mode == 0)
	    len += (cp - start);
    }	
}
    return (start);
}


void
access_list_out (int num, void_fn_t fn) {
    condition_t *condition;
  
    if (access_list[num] == NULL)
        return;

    LL_Iterate (access_list[num], condition) {
        char *tag;

        if (condition->permit) 
	    tag = "permit";
        else
	    tag = "deny";

        if (condition->prefix == NULL) {
            fn ("access-list %d %s all\n", num, tag);
	}
        else {
	    char options[MAXLINE], *cp = options;

	    *cp = '\0';
	    if (condition->wildcard) {
		*cp++ = ' ';
		prefix_toa2 (condition->wildcard, cp);
	        cp += strlen (cp);
	    }
            if (condition->exact) {
	        strcpy (cp, " exact");
	        cp += strlen (cp);
	    }
            if (condition->refine) {
	        strcpy (cp, " refine");
	        cp += strlen (cp);
	    }
            fn ("access-list %d %s %p%s\n", num, tag, condition->prefix, 
		options);
	}
    }	
}


/* this will be obsolete */
char *access_list_toa (void) {
  char *cp, *start;
  int i;
  
  start = NewArray (char, 1024*4); /* XXX */
  cp = start;

  for (i=0; i < MAX_ALIST; i++) {
    char *stmp;
    if (access_list[i] == NULL) continue;
    stmp = access_list_toa2 (i);
    sprintf (cp, "%s", stmp);
    Delete (stmp);
    cp += strlen (cp);
  }
  return (start);
}
