#ifndef STM32_USB_DEV_H
#define STM32_USB_DEV_H

#include <QObject>
#include <QDebug>
#include <libusb-1.0/libusb.h>
#include <unistd.h>
#include <fcntl.h>
#include <QStringList>
#include <QThread>

/* Message type */
#define MSG_REQUEST_DATA	0x2001
#define MSG_GOTO_APP		0x2002
#define MSG_PROGRAM_DATA    0x2003
#define MSG_DEV_ERASE		0x2004
/* Error Code */
#define	MSG_SUCCESS			0x3230
#define MSG_INVALID			0x3231
#define MSG_WFORMAT			0x3232
#define MSG_FAILED			0x3233
#define MSG_WRONG_CRC		0x3234
/* Type Definition */
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
/* Packet Structure (Protocol) */
struct task_struct {
    u8 msg_head[2];
    u16 msg_error;
    u16 msg_type;
    u16 data_length;
    u8 data[52];
    u16 crc;
    u8 msg_tail[2];
} __attribute__((packed));

class FirmwareUpdateWorker : public QObject {
    Q_OBJECT
public:
    explicit FirmwareUpdateWorker(QObject *parent = nullptr)
        : QObject(parent), total_len(0)
    {
        memset(&send_task, 0x00, sizeof(struct task_struct));
        memset(&recv_task, 0x00, sizeof(struct task_struct));
    }

    void startUpdate(QString dev, QString hex);
private:
    int dev_desc;
    int hex_desc;
    u64 total_len;
    struct task_struct send_task;
    struct task_struct recv_task;
    void update_fw(void);
    int usb_request(u16 msg_type, const u8 *data, u16 data_len);
    int usb_recv(void);
    int usb_err_check(void);
    void ascii_2_hex(char *asc_code, unsigned char *hex_code, unsigned int len);
signals:
    void progressChanged(uint8_t progress);
    void updateCompleted();
    void eraseCompleted();
};

class stm32_usb_dev : public QObject
{
    Q_OBJECT
public:
    explicit stm32_usb_dev(QObject *parent = nullptr);
    ~stm32_usb_dev() {
        libusb_exit(nullptr);
    }
    Q_INVOKABLE QStringList findStm32Devices(void);
    Q_INVOKABLE void setProgress(uint8_t prog);
    Q_INVOKABLE uint8_t getProgress(void);
    Q_INVOKABLE void startUpdateFirmware(QString dev, QString hex);

private:
    /* Private Variable */
    int dev_desc;
    int hex_desc;
    u64 total_len;
    uint8_t update_progress;
    struct task_struct send_task;
    struct task_struct recv_task;
    /* Private Method */
    int usb_request(u16 msg_type, const u8 *data, u16 data_len);
    int usb_recv(void);
    int usb_err_check(void);
signals:
    void progressChanged(uint8_t progress);
    void updateCompleted();
    void eraseCompleted();
};

#endif // STM32_USB_DEV_H
