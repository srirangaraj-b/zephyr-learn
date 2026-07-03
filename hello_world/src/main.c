#include <zephyr/kernel.h>

int main(){
    while(1){
    printk("Hello World!!\n");
    k_msleep(1000);
    }
}