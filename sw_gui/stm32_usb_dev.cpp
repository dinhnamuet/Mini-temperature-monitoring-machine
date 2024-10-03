#include "stm32_usb_dev.h"
#include <QDir>
#include "CRC.h"
#include <stdio.h>
#include <sys/stat.h>

#define TIMING  50000

stm32_usb_dev::stm32_usb_dev(QObject *parent)
    : QObject{parent}, update_progress(0), total_len(0)
{
    libusb_init(nullptr);
}

QStringList stm32_usb_dev::findStm32Devices(void) {
    QDir devDir("/dev");
    QStringList filters;
    filters << "stm32-*";
    QStringList fileList = devDir.entryList(filters, QDir::Files | QDir::System);
    return fileList;
}

void stm32_usb_dev::setProgress(uint8_t prog) {
    if (update_progress == prog) {
        return;
    } else {
        update_progress = prog;
        emit progressChanged(update_progress);
    }
}

uint8_t stm32_usb_dev::getProgress() {
    return update_progress;
}

void stm32_usb_dev::startUpdateFirmware(QString dev, QString hex){
    QString devfs = "/dev/" + dev;
    hex.replace("file://", "");
    QThread *thread = QThread::create([=]() {
        FirmwareUpdateWorker worker;
        connect(&worker, &FirmwareUpdateWorker::updateCompleted, this, &::stm32_usb_dev::updateCompleted);
        connect(&worker, &FirmwareUpdateWorker::progressChanged, this, &stm32_usb_dev::setProgress);
        connect(&worker, &FirmwareUpdateWorker::eraseCompleted, this, &stm32_usb_dev::eraseCompleted);
        worker.startUpdate(devfs, hex);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();

}

void FirmwareUpdateWorker::update_fw(){
    char buffer[1024] = { 0 };
    char line_rd[50] = {0};
    ssize_t bytesRead;
    int lineLength = 0;
    u8 hex[50] = { 0 };
    u32 prog = 0;
    struct stat hex_file;

    if (fstat(hex_desc, &hex_file) < 0) {
        goto exit;
    }
    while ((bytesRead = read(hex_desc, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytesRead] = '\0';

        for (int i = 0; i < bytesRead; i++) {
            if (buffer[i] == '\n') {
                line_rd[lineLength - 1] = '\0';
                ascii_2_hex(line_rd, hex, strlen(line_rd));
                total_len += strlen(line_rd) + 1;
                prog = total_len * 100 / hex_file.st_size;
                qDebug()<<"Writting: "<<total_len<<"/"<<hex_file.st_size<<" bytes of hex file!";
                emit progressChanged((u8)prog);
                if (usb_request(MSG_PROGRAM_DATA, hex, strlen(line_rd) / 2) < 0) {
                    qDebug()<<__func__<<", "<<__LINE__;
                    goto exit;
                }
                if (usb_recv() < 0) {
                    qDebug()<<__func__<<", "<<__LINE__;
                    goto exit;
                }
                if (usb_err_check() < 0) {
                    goto exit;
                } else {
                    if (recv_task.msg_error == MSG_SUCCESS) {
                        qDebug()<<("Device send ACK!");
                    } else {
                        qDebug()<<"Device send NACK, Error Code: "<<recv_task.msg_error;
                    }
                    usleep(TIMING);
                }
                lineLength = 0;
            } else if (buffer[i] == ':') {
                lineLength = 0;
                total_len ++;
            } else {
                line_rd[lineLength] = buffer[i];
                lineLength++;
            }
        }
    }

    if (bytesRead == -1) {
        perror("Error reading file");
        goto exit;
    }

    if (lineLength) {
        line_rd[lineLength] = '\0';
        ascii_2_hex(line_rd, hex, strlen(line_rd));
        total_len += strlen(line_rd) + 1;
        prog = total_len * 100 / hex_file.st_size;
        qDebug()<<"Writting: "<<total_len<<"/"<<hex_file.st_size<<" bytes of hex file!";
        emit progressChanged((u8)prog);
        if (usb_request(MSG_PROGRAM_DATA, hex, strlen(line_rd) / 2) < 0) {
            qDebug()<<__func__<<", "<<__LINE__;
            goto exit;
        }
        if (usb_recv() < 0) {
            qDebug()<<__func__<<", "<<__LINE__;
            goto exit;
        }
        if (usb_err_check() < 0) {
            goto exit;
        } else {
            if (recv_task.msg_error == MSG_SUCCESS) {
                qDebug()<<("Device send ACK!");
            } else {
                qDebug()<<"Device send NACK, Error Code: "<<recv_task.msg_error;
            }
            usleep(TIMING);
        }
    }
    usb_request(MSG_GOTO_APP, NULL, 0);
    emit updateCompleted();
exit:
    close(dev_desc);
    close(hex_desc);
}

int stm32_usb_dev::usb_request(u16 msg_type, const u8 *data, u16 data_len) {
    send_task.msg_head[0]       = 0xFA;
    send_task.msg_head[1]       = 0xFB;
    send_task.msg_error         = MSG_SUCCESS;
    send_task.msg_type          = msg_type;
    send_task.data_length       = data_len;
    memcpy(send_task.data, data, data_len);
    send_task.crc               = CRC_CalculateCRC16(send_task.data, send_task.data_length);
    send_task.msg_tail[0]       = 0xFC;
    send_task.msg_tail[1]       = 0xFD;

    return write(dev_desc, &send_task, sizeof(struct task_struct));
}
int FirmwareUpdateWorker::usb_request(u16 msg_type, const u8 *data, u16 data_len) {
    send_task.msg_head[0]       = 0xFA;
    send_task.msg_head[1]       = 0xFB;
    send_task.msg_error         = MSG_SUCCESS;
    send_task.msg_type          = msg_type;
    send_task.data_length       = data_len;
    memcpy(send_task.data, data, data_len);
    send_task.crc               = CRC_CalculateCRC16(send_task.data, send_task.data_length);
    send_task.msg_tail[0]       = 0xFC;
    send_task.msg_tail[1]       = 0xFD;

    return write(dev_desc, &send_task, sizeof(struct task_struct));
}

int stm32_usb_dev::usb_recv() {
    return read(dev_desc, &recv_task, sizeof(struct task_struct));
}
int FirmwareUpdateWorker::usb_recv() {
    return read(dev_desc, &recv_task, sizeof(struct task_struct));
}

int stm32_usb_dev::usb_err_check() {
    if (recv_task.msg_head[0] != 0xFA || recv_task.msg_head[1] != 0xFB || recv_task.msg_tail[0] != 0xFC || recv_task.msg_tail[1] != 0xFD) {
        return -1;
    } else if (CRC_CalculateCRC16(recv_task.data, recv_task.data_length) != recv_task.crc) {
        return -1;
    } else if (recv_task.msg_error != MSG_SUCCESS) {
        return -1;
    } else {
        return 0;
    }
}
int FirmwareUpdateWorker::usb_err_check() {
    if (recv_task.msg_head[0] != 0xFA || recv_task.msg_head[1] != 0xFB || recv_task.msg_tail[0] != 0xFC || recv_task.msg_tail[1] != 0xFD) {
        return -1;
    } else if (CRC_CalculateCRC16(recv_task.data, recv_task.data_length) != recv_task.crc) {
        return -1;
    } else if (recv_task.msg_error != MSG_SUCCESS) {
        return -1;
    } else {
        return 0;
    }
}

void FirmwareUpdateWorker::ascii_2_hex(char *asc_code, unsigned char *hex_code, unsigned int len) {
    int idx = 0;
    for (int i = 0; i < len; i += 2) {
        sscanf(&asc_code[i], "%2hhx", &hex_code[idx]);
        idx++;
    }
}

void FirmwareUpdateWorker::startUpdate(QString dev, QString hex) {
    u8 try_count = 8;
    dev_desc = open(dev.toUtf8().constData(), O_RDWR);
    if (-1 == dev_desc) {
        qDebug()<<"Open "<<dev.toUtf8().constData()<<" failed!";
        return;
    }
    hex_desc = open(hex.toUtf8().constData(), O_RDONLY);
    if (-1 == hex_desc) {
        qDebug()<<"Open "<<dev.toUtf8().constData()<<" failed!";
        close(dev_desc);
        return;
    }
retry:
    if (--try_count == 0)
        goto exit;
    if (usb_request(MSG_DEV_ERASE, NULL, 0) < 0) {
        qDebug()<<__func__<<", "<<__LINE__;
        goto retry;
    }
    if (usb_recv() < 0) {
        qDebug()<<__func__<<", "<<__LINE__;
        goto retry;
    }
    if (usb_err_check() < 0) {
        qDebug()<<__func__<<", "<<__LINE__;
        goto retry;
    } else {
        if (recv_task.msg_error == MSG_SUCCESS) {
            emit eraseCompleted();
        } else {
            qDebug()<<__func__<<", "<<__LINE__;
            goto retry;
        }
        sleep(2);
    }
    update_fw();
exit:
    close(dev_desc);
    close(hex_desc);
}
