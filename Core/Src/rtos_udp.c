#include <math.h>
#include <rtos_udp.h>
#include <string.h>
static struct receive_data ros_data = { };
static struct send_data f7_data = { };
struct timeval tv;
#define F7_ADDR "192.168.000.020"
#define PC_ADDR "172.16.233.145"
#define F7_PORT 7777
#define PC_PORT 54321

osThreadId controllerTaskHandle;
uint32_t controllerTaskBuffer[512];
osStaticThreadDef_t controllerTaskControlBlock;
void UDPSendReceive(void const *argument);

void UDPDefineTasks() {
	osThreadStaticDef(controllerTask, UDPSendReceive, osPriorityNormal, 0,
			512, controllerTaskBuffer, &controllerTaskControlBlock);
	controllerTaskHandle = osThreadCreate(osThread(controllerTask), NULL);
}

void UDPSendReceive(void const *argument) {
	fd_set rset;
	int sock;
	int iBytesWritten = 0;
	char buffer[512]; //最大受信データサイズ
	char txbuf[256]; //最大送信データサイズ
	struct sockaddr_in myAddr, cAddr;
	sock = lwip_socket(AF_INET, SOCK_DGRAM, 0); //udp制御ブロックを作成
	memset((char*) &myAddr, 0, sizeof(myAddr)); //myAddrを0で埋める
	myAddr.sin_family = AF_INET;
	myAddr.sin_len = sizeof(myAddr);
	//myAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	myAddr.sin_addr.s_addr = inet_addr(F7_ADDR); //マイコンのIP
	myAddr.sin_port = htons(F7_PORT); //マイコンのポート
	FD_ZERO(&rset); //集合の初期化
	int err = lwip_bind(sock, (struct sockaddr*) &myAddr, sizeof(myAddr)); //制御ブロックをIPとポートに紐づける。
	if (err != 0) {
		printf("UDPController:ERROR \r\n");
	} else {
		printf("UDPController:Socket Opened!\r\n");
	}
	int timeout = 0;
	struct receive_data data_null; //受信を失敗したとき用のデータ
	bzero(&data_null, sizeof(struct receive_data)); //初期化
	int maxfdp1 = sock + 1;
	cAddr.sin_family = AF_INET;
	cAddr.sin_len = sizeof(cAddr);
	cAddr.sin_addr.s_addr = inet_addr(PC_ADDR); //PCのIP
	cAddr.sin_port = htons(PC_PORT); //PCのポート


	/*---------test sumple----------*/
	f7_data.omni_x = 0.5f;
	f7_data.omni_y = 0.5f;
	f7_data.hat_shoulder_success = 0;
	f7_data.sword_A_shoulder_success = 0;
	f7_data.sword_B_shoulder_success = 0;
	f7_data.launcher_linear_success = 0;
	/*------------------------*/

	while (1) {
		FD_SET(sock, &rset);
		tv.tv_usec = 1000;
		select(maxfdp1, &rset, NULL, NULL, &tv); //1秒間イベント発生を待つ
		if (timeout < 100) {
			timeout++;
		} else {
			memcpy(&ros_data, &data_null, sizeof(struct receive_data)); //100回ループしたら0データを渡す。
		}
		if (FD_ISSET(sock, &rset)) {
			socklen_t n;
			socklen_t len = sizeof(cAddr);
			struct send_data* sd = (struct send_data*)&txbuf; //txbufの位置に重ねてsdを宣言
			memcpy(sd, &f7_data, sizeof(struct send_data)); //送信データをコピーする
			iBytesWritten = lwip_sendto(sock, (char*)txbuf, sizeof(txbuf), 0, (struct sockaddr*)&cAddr, sizeof(cAddr));
			//iBytesWritten = lwip_sendto(sock, (char*)"hello world from f7!", 256, 0, (struct sockaddr*)&cAddr, sizeof(cAddr));
			n = lwip_recvfrom(sock, (char*) buffer, sizeof(buffer), (int) NULL, (struct sockaddr*) &cAddr, &len); //bufferに受信データを格納
			if (n > 0) {
				timeout = 0;
				if (n < sizeof(struct receive_data)) {
					printf("invalid data : \r\n"); //データが欠損しているのでループを送る
					continue;
				}
				struct receive_data *d = (struct receive_data*) &buffer; //bufferの位置にreceive_data構造体を作る。(疑似的にデータが変換される)
				memcpy(&ros_data, d, sizeof(struct receive_data)); //受信データをコピーする
			}
		}
		osDelay(10);
	}
}
