#include "removeCharFromString.h"
char *removeCharFromString(char charToRemove, char *str){
int i,j;
i = 0;
while(i<strlen(str))
{
    if (str[i]==charToRemove)
    {
        for (j=i; j<strlen(str); j++) {
            str[j]=str[j+1];
        }
    }
    else i++;
}
return str;
}