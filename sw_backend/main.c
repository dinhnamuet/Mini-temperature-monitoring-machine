#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "protocol.h"

#define BUFFER_SIZE 1024

void ascii_2_hex(char *asc_code, unsigned char *hex_code, unsigned int len)
{
    int idx = 0;
    for (int i = 0; i < len; i += 2)
    {
        sscanf(&asc_code[i], "%2hhx", &hex_code[idx]);
        idx++;
    }
}

int main(int argc, char *argv[])
{
    char buffer[BUFFER_SIZE];
    char line_rd[50] = {0};
    ssize_t bytesRead;
    int lineLength = 0;
    struct task_struct send_task, recv_task;
    int stm32_fd, hex_fd;
    u8 buf[54];
    u8 hex[50];
    u64 total_len = 0x00;
    struct stat hex_file;

    if (argc < 3)
    {
        puts("./update_firmware + <path-to-device-file> + <hex-file-name>");
        return -1;
    }
    stm32_fd = open(argv[1], O_RDWR);
    if (-1 == stm32_fd)
    {
        perror("Error: ");
        return -1;
    }
    hex_fd = open(argv[2], O_RDONLY);
    if (-1 == hex_fd)
    {
        perror("Error opening file");
        close(stm32_fd);
        return -1;
    }
    if (fstat(hex_fd, &hex_file) < 0)
    {
        goto exit;
    }

    while ((bytesRead = read(hex_fd, buffer, sizeof(buffer) - 1)) > 0)
    {
        buffer[bytesRead] = '\0';

        for (int i = 0; i < bytesRead; i++)
        {
            if (buffer[i] == '\n')
            {
                line_rd[lineLength - 1] = '\0';
                ascii_2_hex(line_rd, hex, strlen(line_rd));
                total_len += strlen(line_rd);
                printf("Writting %ld/%ld bytes of hex file!\n", total_len, hex_file.st_size);
                if (usb_request(stm32_fd, &send_task, MSG_PROGRAM_DATA, hex, strlen(line_rd) / 2) < 0)
                {
                    perror("Error: ");
                    goto exit;
                }
                if (usb_recv(stm32_fd, &recv_task) < 0)
                {
                    perror("Error: ");
                    goto exit;
                }
                if (usb_err_check(&recv_task) < 0)
                {
                    goto exit;
                }
                else
                {
                    if (recv_task.msg_error == MSG_SUCCESS)
                    {
                        puts("Device send ACK!");
                    }
                    else
                    {
                        puts("Device send NACK!");
                    }
                }
                sleep(0.5);
                lineLength = 0;
            }
            else if (buffer[i] == ':')
            {
                lineLength = 0;
            }
            else
            {
                line_rd[lineLength] = buffer[i];
                lineLength++;
            }
        }
    }

    if (bytesRead == -1)
    {
        perror("Error reading file");
        close(hex_fd);
        return -1;
    }

    if (lineLength)
    {
        line_rd[lineLength] = '\0';
        ascii_2_hex(line_rd, hex, strlen(line_rd));
        total_len += strlen(line_rd);
        printf("Writting %ld/%ld bytes of hex file!\n", total_len, hex_file.st_size);
        if (usb_request(stm32_fd, &send_task, MSG_PROGRAM_DATA, hex, strlen(line_rd) / 2) < 0)
        {
            perror("Error: ");
            goto exit;
        }
        if (usb_recv(stm32_fd, &recv_task) < 0)
        {
            perror("Error: ");
            goto exit;
        }
        if (usb_err_check(&recv_task) < 0)
        {
            goto exit;
        }
        else
        {
            if (recv_task.msg_error == MSG_SUCCESS)
            {
                puts("Device send ACK!");
            }
            else
            {
                puts("Device send NACK!");
            }
        }
    }
    sleep(0.5);
    usb_request(stm32_fd, &send_task, MSG_GOTO_APP, NULL, 0);
exit:
    close(stm32_fd);
    close(hex_fd);
    return 0;
}
