#include <math.h>
#include <rtos_udp.h>
#include <string.h>
static struct receive_data ros_data = { };
static struct send_data f7_data = { };
struct timeval tv;
#define F7_ADDR "192.168.000.020"
#define PC_ADDR "192.168.0.100"
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
	printf("ender udp task\r\n");
	fd_set reading;
	int sock1;
	int iBytesWritten = 0;
	char rxbuf[512]; //最大受信データサイズ
	char txbuf[256]; //最大送信データサイズ
	struct sockaddr_in myAddr, cAddr;
	sock1 = lwip_socket(AF_INET, SOCK_DGRAM, 0); //udp制御ブロックを作成

	//ソケットを作成
	memset((char*) &myAddr, 0, sizeof(myAddr)); //myAddrを0で埋める
	memset((char*) &cAddr, 0, sizeof(cAddr));
	myAddr.sin_family = AF_INET;
	myAddr.sin_len = sizeof(myAddr);
	myAddr.sin_addr.s_addr = inet_addr(F7_ADDR); //マイコンのIP
	myAddr.sin_port = htons(F7_PORT); //マイコンのポート
	cAddr.sin_family = AF_INET;
	cAddr.sin_len = sizeof(cAddr);
	cAddr.sin_addr.s_addr = inet_addr(PC_ADDR); //PCのIP
	cAddr.sin_port = htons(PC_PORT); //PCのポート

	int err = lwip_bind(sock1, (struct sockaddr*) &cAddr, sizeof(cAddr)); //制御ブロックを送信先のIPとポートに紐づける。
	if (err != 0) {
		printf("UDPController:ERROR \r\n");
	} else {
		printf("UDPController:Socket Opened!\r\n");
	}
	int maxfd = 0;
	int result = 0;
	/*---------test sumple----------*/
	f7_data.omni_x = 0.5f;
	f7_data.omni_y = 0.5f;
	f7_data.hat_shoulder_success = 0;
	f7_data.sword_A_shoulder_success = 0;
	f7_data.sword_B_shoulder_success = 0;
	f7_data.launcher_linear_success = 0;
	/*------------------------*/

	printf("enter udp loop\r\n");
	while (1) {
		FD_ZERO(&reading); //ディスクリプタ集合の初期化
		FD_SET(sock1, &reading); //readingにsock（ディスクリプタ番号）を追加
		maxfd = sock1;
		maxfd ++;
		memset(&tv, 0, sizeof(tv));
		//tv.tv_usec = 1000;
		tv.tv_sec = 5;
		result = select(maxfd, &reading, NULL, NULL, &tv); //ファイルディスクリプタ―がready状態になるまで1ミリ秒まで待つ。
		printf("select result: %d\r\n", result);
		if (result == -1){
			//error
			continue;
		}
		if (result == 0){
			//timeout
			continue;
		}
		if (FD_ISSET(sock1, &reading)) { //readingの中にsockの値が含まれているか調べる。
			printf("fd is set\r\n");
			socklen_t n;
			socklen_t len = sizeof(cAddr);
			n = lwip_recvfrom(sock1, (char*) rxbuf, sizeof(rxbuf), (int) NULL, (struct sockaddr*) &cAddr, &len); //rxbufに受信データを格納
			if (n > 0) {
				if (n < sizeof(struct receive_data)) {
					printf("invalid data : \r\n"); //データが欠損しているのでループを送る
					continue;
				}
				struct receive_data *d = (struct receive_data*) &rxbuf; //rxbufの位置にreceive_data構造体を作る。(疑似的にデータが変換される)
				memcpy(&ros_data, d, sizeof(struct receive_data)); //受信データをコピーする
				printf("omni1:%f\r\nomni2:%f\r\nomni3:%f\r\nomni4:%f\r\n", ros_data.omni1_power, ros_data.omni2_power, ros_data.omni3_power, ros_data.omni4_power);

				//callback
				struct send_data* sd = (struct send_data*)&txbuf; //txbufの位置に重ねてsdを宣言
				memcpy(sd, &f7_data, sizeof(struct send_data)); //送信データをコピーする
				iBytesWritten = lwip_sendto(sock1, (char*)txbuf, sizeof(txbuf), 0, (struct sockaddr*)&cAddr, sizeof(cAddr));
				//iBytesWritten = lwip_sendto(sock, (char*)"hello world from f7!", 256, 0, (struct sockaddr*)&cAddr, sizeof(cAddr));
				if (iBytesWritten > 0) {
					printf("success send data\r\n");
					if (iBytesWritten < sizeof(struct send_data)) {
						printf("send data invalid\r\n");
					}else{
						printf("send data is perfect\r\n");
					}
				}else{
					printf("failed send data\r\n");
				}
			}
		}
		osDelay(10);
	}
}
