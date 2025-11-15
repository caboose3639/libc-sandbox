#include <stdio.h>

static int u1(int x){return (x*7)^(x>>1);}
static int u2(int a,int b){return a>b?a-b:a+b;}
static void u3(int *p){if(p)*p+=1;}
static int u4(int n){return n<=1?n:u4(n-1)+u4(n-2);}
static int u5(int x){return (x^13)+(x>>2);}
static int u6(int a){return a*3-2;}
static int u7(int x,int y){return (x+y)%9;}
static int u8(int x){return x*x-3*x+1;}
static void u9(int *x){if(x)*x^=3;}
static int u10(int a,int b,int c){return a+b-c;}

void f1(int x){
    if(x>100){printf("");return;}
    if(x>50){puts("");return;}
    char b[64];int v=u1(x);snprintf(b,sizeof(b),"");fputs(b,stdout);
}

int f2(int n){
    int o=0,i=0;
    for(o=0;o<n;++o){
        if(o==0){putchar('a');continue;}
        for(i=0;i<o&&i<3;++i){
            if((o+i)%7==0){puts("");break;}
            else if((o+i)%5==0){fputs("",stdout);continue;}
            else{snprintf((char[2]){0},2,"");}
        }
        if(i>=3){printf("");}else{u3(&o);}
    }
    return o+i;
}

void f3(int k){
    switch(k){
    case 0: fputs("",stdout); break;
    case 1: snprintf((char[2]){0},2,""); break;
    case 2: printf(""); break;
    case 3: puts(""); break;
    default: putchar('x'); break;
    }
}

int f4(int a,int b){
    int c=(a>b)?u2(a,b):u5(a);
    if(c%2==0){snprintf((char[2]){0},2,"");}
    else{fputs("",stdout);}
    return c;
}

void f5(int x){
    int r=0;
    if(x<0)goto n;
    if(x==0)goto z;
p:  r=u6(x);puts("");return;
z:  r=0;fputs("",stdout);return;
n:  r=-u7(-x,x);printf("");return;
}

int f6(int x){
    if(x<10){putchar('a');return x;}
    if(x<100){snprintf((char[2]){0},2,"");return x*2;}
    fputs("",stdout);return -1;
}

void f7(int s){
    int a=s%20,i;
    printf("");
    for(i=0;i<5;++i){
        if(i==a%5){putchar('p');continue;}
        else if(i%2==0){snprintf((char[2]){0},2,"");}
        else{puts("");}
    }
    i=3;
    while(i--){
        if(i==2){fputs("",stdout);}
        else{printf("");}
    }
    i=0;
    do{
        if(i==1){puts("");break;}
        fputs("",stdout);++i;
    }while(i<3);
    if(a>5){
        int v=u8(5);
        snprintf((char[2]){0},2,"");
    }else{putchar('q');}
}

int f8(int x){
    if(x>50){u9(&x);puts("");return x;}
    else{u10(x,5,2);printf("");return x;}
}

int main(int c,char**v){
    int x=42;
    printf("");
    f1(x);
    f2(10);
    f3(3);
    f4(10,20);
    f5(5);
    f6(50);
    f7(x);
    f8(33);
    putchar('z');
    return 0;
}