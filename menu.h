#ifndef _Menu_H
#define _Menu_H

#include <stdio.h>

typedef struct MenuNode {
	struct MenuNode *prev;
	struct MenuNode *next;
	struct MenuNode *parent;
	struct MenuNode *child;
	char *name;
	void (*executing)(void);
} MenuNode;

void init_menu(void);
int nextMenuItem(void);
int prevMenuItem(void);
int childMenuItem(void);
char *getCurrentName(void);
int parentMenuItem(void);
int menuAction(void);
int nodeCounter(void);

MenuNode *Level1Node(char s[17], MenuNode *par, MenuNode *chil,void * ex);
MenuNode *ChildNode(char s[17], MenuNode *par, MenuNode *chil, void * ex);

short menuItemIndex;

#endif /* _Menu_H */
/*  ����  End Of File  �������� �������������������������������������������� */