#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "hw_config.h"

#define min(a,b) ((a) < (b) ? (a) : (b))
#define WRITE_INST 0

int bus_width, al, pc, scr, is_depth, os_depth, freq;
char item[100];

hwc hw;
Config config;

int load_is(
    int num_rows,
    int in_pos_h,
    int input_addr_base //actural address = input_addr_base + (h * input_width + w) * acc_times_c + c
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
    bus_width = 128;
    al = 64;
    pc = 8;
    scr = 8;
    is_depth = 1024;
    os_depth = 1024;
    freq = 500;
    InitConfig(&config, bus_width, al, pc, scr, is_depth, os_depth, freq);
    Inithwc(&hw, config);
    conv2d(112,64,128,3,1,2,0,0,0);

    return 0;
}

int load_is(    
    int num_rows,
    int in_pos_h,
    int input_addr_base) 
{
    int in_pos_w = 0;
    int in_pos_c = 0;
    int input_addr_bias = 0;
    for (int i_rows = 0; i_rows < num_rows; ++i_rows) {//一个row对应一个addr
        input_addr_bias = i_rows;
        for (int j_reg = hw.InputSRAMWidth / hw.BusWidth - 1; j_reg >= 0; --j_reg) {
            if (j_reg != 0) {
                sprintf(item, "Linp\t <pos> %d\t <is_addr> %d\t <input_map> %d\n", j_reg, i_rows, input_map_position);
                PushInstStack(&inst_stack, item, 0, 0);
            } else {
                sprintf(item, "Lin\t\t <pos> %d\t <is_addr> %d\t <input_map> %d\n", j_reg, i_rows, input_map_position);
                PushInstStack(&inst_stack, item, 0, 0);
            }
            input_map_position -= (config.BUS_WIDTH / config.DATA_WIDTH);
            // 注意: 每个channel的最后一行可能需要特殊处理
        }
        input_map_position += (acc0.InputSRAMWidth / config.DATA_WIDTH) + (config.BUS_WIDTH / config.DATA_WIDTH);
    }
    return input_map_position;
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
){
    //basic calculation
    int input_height = input_size;
    int input_width = input_size;
    int output_size = (input_size + 2 * padding - kernel_size) / stride + 1;

    int acc_times_h = kernel_size;
    int acc_times_w = kernel_size;
    int acc_times_c = ceil((float)input_channel / hw.AL);

    int acc_times = acc_times_c * acc_times_h * acc_times_w;
    int para_times = ceil((float)output_channel / hw.PC);

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


    // 释放内存
    free(IS_load_columns);
    free(weight_update_ls);
    //return 0;
};