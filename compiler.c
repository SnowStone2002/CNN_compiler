#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "hw_config.h"
#include "inst_stack.h"

#define min(a,b) ((a) < (b) ? (a) : (b))
#define WRITE_INST 0

int bus_width, al, pc, scr, is_depth, os_depth, freq;
char item[100];

InstStack inst_stack;
hwc hw;
Config config;

int input_height;
int input_width;
int kernel_height;
int kernel_width;
int output_size;
int output_width;
int output_height;
int acc_times_h;
int acc_times_w;
int acc_times_c;
int acc_times;
int para_times;

int load_is(
    int num_rows,
    int in_pos_h,
    int input_addr_base //actural address = input_addr_base + (h * input_width + w) * acc_times_c + c
);

int load_cim(
    int num_ls,
    int num_channel,
    int* wt_pos_n,
    int* wt_pos_hwc
);

int load_bias(
    int num_channel,
    int* wt_pos_n
);

int load_os(
    int num_rows,
    int in_pos_h,
    int input_addr_base //actural address = input_addr_base + (h * input_width + w) * acc_times_c + c
);

void set_mul(
    int multiply_num
);

void conv2d(
    int input_size,
    int input_channel,
    int output_channel,
    int kernel_size,
    int stride,
    int padding,
    int input_addr_base,
    int weight_addr_base,
    int output_addr_base
);

int main(){
    bus_width = 64;
    al = 64;
    pc = 64;
    scr = 16;
    is_depth = 256;
    os_depth = 256;
    freq = 500;
    InitConfig(&config, bus_width, al, pc, scr, is_depth, os_depth, freq);
    PrintConfig(&config);
    Inithwc(&hw, config);
    Printhwc(&hw);
    InitInstStack(&inst_stack, 10, "inst.txt");
    conv2d(56,128,256,3,1,2,0,0,0);

    return 0;
}

int load_is(    
    int num_rows,
    int in_pos_h,
    int input_addr_base
) {
    int in_pos_h_0 = in_pos_h;
    int in_pos_w = 0;
    int in_pos_c = 0;
    int input_addr_bias = 0;
    for (int i_rows = 0; i_rows < num_rows; ++i_rows) {//一个row对应一个addr
        input_addr_bias = i_rows;       //actural address = input_addr_base + (h * input_width + w) * acc_times_c + c
        in_pos_c = input_addr_bias % acc_times_c;
        in_pos_w = input_addr_bias / acc_times_c % input_width;
        in_pos_h = in_pos_h_0 + input_addr_bias / acc_times_c / input_width;
        for (int j_reg = hw.InputSRAMWidth / hw.BusWidth - 1; j_reg >= 0; --j_reg) {
            if (j_reg != 0) {
                sprintf(item, "Linp\t <pos> %d\t <is_addr> %d\t <h> %d\t <w> %d\t <c> %d\n", j_reg, i_rows, in_pos_h, in_pos_w, in_pos_c);
                PushInstStack(&inst_stack, item, 0, 0);
            } else {
                sprintf(item, "Lin\t\t <pos> %d\t <is_addr> %d\t <h> %d\t <w> %d\t <c> %d\n", j_reg, i_rows, in_pos_h, in_pos_w, in_pos_c);
                PushInstStack(&inst_stack, item, 0, 0);
            }
        }
    }
    return in_pos_h;
}

int load_cim(
    int num_ls,
    int num_channel,
    int* wt_pos_n,
    int* wt_pos_hwc
) {
    int wt_pos_hwc_0 = *wt_pos_hwc;
    int wt_pos_n_0 = *wt_pos_n;
    int wt_pos_h;
    int wt_pos_w;
    int wt_pos_c;
    for (int i_channel = 0; i_channel < num_channel; i_channel++){
        for (int j_ls = 0; j_ls < num_ls; j_ls++){//input_addr_base + (h * input_width + w) * acc_times_c + c
            *wt_pos_hwc = j_ls+wt_pos_hwc_0;
            *wt_pos_n = wt_pos_n_0 + i_channel;
            wt_pos_h = *wt_pos_hwc / acc_times_c / kernel_width;
            wt_pos_w = *wt_pos_hwc / acc_times_c % kernel_width;
            wt_pos_c = *wt_pos_hwc % acc_times_c;
            for (int k_reg = hw.CIMsWriteWidth / hw.BusWidth * config.WEIGHT_ROW - 1; k_reg >= 0; k_reg--){
                int row_reg = (hw.CIMsWriteWidth / hw.BusWidth * config.WEIGHT_ROW - 1 - k_reg) / (hw.CIMsWriteWidth / hw.BusWidth);
                int pause_reg = k_reg % (hw.CIMsWriteWidth / hw.BusWidth);
                if (pause_reg == 0) {
                    sprintf(item, "Lwt\t\t <pos> %d\t <cm_addr> %d\t <n> %d\t <h> %d\t <w> %d\t <c> %d\n", 
                    k_reg % (hw.CIMsWriteWidth / hw.BusWidth), 
                    i_channel * config.SCR * config.WEIGHT_ROW + row_reg * config.SCR + j_ls, 
                    *wt_pos_n, wt_pos_h, wt_pos_w, wt_pos_c);
                    PushInstStack(&inst_stack, item, 0, 0);
                } else {
                    sprintf(item, "Lwtp\t <pos> %d\t <cm_addr> %d\t <n> %d\t <h> %d\t <w> %d\t <c> %d\n", 
                    k_reg % (hw.CIMsWriteWidth / hw.BusWidth), 
                    i_channel * config.SCR * config.WEIGHT_ROW + row_reg * config.SCR + j_ls,
                    *wt_pos_n, wt_pos_h, wt_pos_w, wt_pos_c);
                    PushInstStack(&inst_stack, item, 0, 0);
                }
            }
        }
    }
    return 1;
}

int load_bias(
    int num_channel,
    int* wt_pos_n
){
    sprintf(item, "Lbs\t\t <n> %d\n", *wt_pos_n);
    PushInstStack(&inst_stack, item, 0, 0);
}

void set_mul(
    int multiply_num
){
    sprintf(item, "Smul\t <num> %d\n", multiply_num);
    PushInstStack(&inst_stack, item, 0, 0);
}

void conv2d(
    int input_size,
    int input_channel,
    int output_channel,
    int kernel_size,
    int stride,
    int padding,
    int input_addr_base,
    int weight_addr_base,
    int output_addr_base
) {
    //basic calculation
    input_height = input_size;
    input_width = input_size;

    kernel_height = kernel_size;
    kernel_width = kernel_size;

    output_size = (input_size + 2 * padding - kernel_size) / stride + 1;
    output_height = output_size;
    output_width = output_size;

    acc_times_h = kernel_size;
    acc_times_w = kernel_size;
    acc_times_c = ceil((float)input_channel / hw.AL);

    acc_times = acc_times_c * acc_times_h * acc_times_w;
    para_times = ceil((float)output_channel / hw.PC);

    int input_height_per_IS_load = hw.InputSRAMDepth / acc_times_c / input_width;
    if (input_height_per_IS_load > input_height)
        input_height_per_IS_load = input_height;
    int IS_load_times_per_conv = (int)ceil((float)input_height / input_height_per_IS_load);

    int* IS_load_heights = (int*)malloc(IS_load_times_per_conv * sizeof(int));

    for (int i = 0; i < IS_load_times_per_conv; ++i) {
        IS_load_heights[i] = input_height_per_IS_load;  // 默认每次载入处理最大行数
    }
    // 如果最后一次载入不能满载，则调整最后一次载入的列数
    if ((input_height) % input_height_per_IS_load != 0) {
        IS_load_heights[IS_load_times_per_conv - 1] = (input_height) % input_height_per_IS_load;
    }

    int weight_update_times_per_conv = (int)ceil((float)acc_times / config.SCR) * para_times;
    int* weight_update_ls = (int*)malloc(weight_update_times_per_conv * sizeof(int));

    // 初始化权重更新次数数组
    int weight_updates_index = 0;
    for (int p = 0; p < para_times; p++) {
        int times_per_para = (int)ceil((float)acc_times / hw.SCR);
        for (int i = 0; i < times_per_para; i++) {
            if (i < times_per_para - 1) {
                // 通常情况，使用满额SCR值
                weight_update_ls[weight_updates_index] = hw.SCR;
                weight_updates_index++;
            } else {
                // 每个参数周期的最后一个元素可能不是满额
                int last_amount = acc_times % hw.SCR;
                if (last_amount == 0) {  // 如果acc_times是config.SCR的倍数，则为满额
                    weight_update_ls[weight_updates_index] = hw.SCR;
                    weight_updates_index++;
                } else {  // 否则，使用余数作为最后一次更新的大小
                    weight_update_ls[weight_updates_index] = last_amount;
                    weight_updates_index++;
                }
            }
        }
    }

    // 验证更新列表是否正确
    // printf("Weight update times per conv:\n");
    // for (int i = 0; i < weight_update_times_per_conv; i++) {
    //     printf("%d ", weight_update_ls[i]);
    // }
    // printf("\n");

    // Main loop
    for (int i = 0; i < IS_load_times_per_conv; ++i) {
        int in_pos_h = i * input_height_per_IS_load;
        load_is(IS_load_heights[i], in_pos_h, input_addr_base);

        for (int j = 0; j < weight_update_times_per_conv; ++j) {
            int wt_pos_n = 0;
            int wt_pos_hwc = 0;
            load_cim(weight_update_ls[j], output_channel, &wt_pos_n, &wt_pos_hwc);

            for (int k = 0; k < config.SCR; ++k) {
                set_mul(k);
                sprintf(item, "Calc\t <scr> %d\n", k);
                PushInstStack(&inst_stack, item, 0, 0);
            }
        }
    }


    // 释放内存
    free(IS_load_heights);
    free(weight_update_ls);
    //return 0;
};