#include <iostream>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <chrono>
#include<fstream>
#include <iomanip>
#include <time.h>
#include <thread>
#include <mutex>
#include <shared_mutex>

#pragma comment(lib, "ws2_32.lib") 
using namespace std;

#define FIN 0x4  // 0100
#define ACK 0x2  // 0010
#define SYN 0x1  // 0001
#define ACK_SYN 0x3  // 0011
#define FIN_ACK 0x6  // 0110
#define OVER 0xF    // 1111

const double MAX_TIME = 0.5 * CLOCKS_PER_SEC;  // �ȴ����ʱ��
const int MAXSIZE = 10240; // ���仺������󳤶�
const int SendPort = 7777;
int WINDOWS = 1;  // ���ڴ�С

struct HEADER {
	uint16_t checksum;  // 16λУ���
	uint16_t length;  // ���ݳ���
	uint8_t seq;  // ���к�
	uint8_t ack;  // ȷ�����к�
	uint8_t flag;  // ��־λ
	uint8_t temp;

	HEADER() {
		temp = 0;
		checksum = 0;
		seq = 0;
		ack = 0;
		flag = 0;
		length = 0;
	}
};
#pragma pack(1)

// ����У���
uint16_t checksum(HEADER header) {
	int size = sizeof(header);
	uint16_t* msg = (uint16_t*)&header;  // ���ṹ����Ϊ 16 λ��������

	int count = (size + 1) / 2;
	u_short* buf = (u_short*)malloc(size + 1);
	memset(buf, 0, size + 1); // ��ʼ��Ϊ0
	memcpy(buf, msg, size);

	uint32_t sum = 0;
	// ѭ���ۼ�ÿ�� 16 λ��
	for (int i = 0; i < count; i++) {
		sum += buf[i];
		if (sum & 0xffff0000) {
			sum &= 0xffff;
			sum++;
		}
	}
	return ~(sum & 0xffff);
}

// �������ֽ�������
bool Connect(SOCKET& send_socket, SOCKADDR_IN& recv_addr, int& recv_addrlen) {
	cout << "---------- ��ʼ�������� ----------" << endl;

	// ��socket��Ϊ������״̬��ʹ��������ִ�У����жϳ�ʱ�ش�
	u_long mode = 1;
	ioctlsocket(send_socket, FIONBIO, &mode);

	// ��һ�����֣�����SYN
	HEADER header1;
	char* buf1 = new char[sizeof(header1)];
	header1.flag = SYN;  // ����flag
	header1.checksum = 0;
	header1.checksum = checksum(header1);  // ����У��� 
	memcpy(buf1, &header1, sizeof(header1));  // ���뻺����
	int tag = sendto(send_socket, buf1, sizeof(header1), 0, (sockaddr*)&recv_addr, recv_addrlen);
	if (tag == -1) {  //��֤���ͽ��
		cout << "��һ�����ַ���ʧ�ܣ�" << endl;
		return false;
	}
	else {
		cout << "��һ�����ֳɹ����� " << endl;
	}
	clock_t start = clock();  // ��¼��һ������ʱ��

	// �ڶ������֣��ȴ�SYN+ACK
	HEADER header2;
	char* buf2 = new char[sizeof(header2)];
	while (true) {
		int recv_len = recvfrom(send_socket, buf2, sizeof(header2), 0, (sockaddr*)&recv_addr, &recv_addrlen);

		// ���յ������flag��У���
		if (recv_len > 0) {
			memcpy(&header2, buf2, sizeof(header2));
			if (header2.flag == ACK_SYN && checksum(header2) == 0) {
				cout << "�յ��ڶ�������" << endl;
				break;
			}
			else {
				cout << "�ڶ����������ݰ�����" << endl;
			}
		}

		// ÿ��ѭ���ж��Ƿ�ʱ
		if (clock() - start > MAX_TIME) {
			cout << "�ȴ��ڶ������ֳ�ʱ���ش���һ������" << endl;
			tag = sendto(send_socket, buf1, sizeof(header1), 0, (sockaddr*)&recv_addr, recv_addrlen);
			if (tag == -1) {  //��֤���ͽ��
				cout << "��һ�������ش�ʧ�ܣ�" << endl;
				return false;
			}
			else {
				cout << "��һ�������Ѿ��ش� " << endl;
			}
			start = clock();  // ��������ʱ��
		}
	}

	// ���������֣�����ACK
	HEADER header3;
	char* buf3 = new char[sizeof(header3)];
	header3.flag = ACK;  // ����flag
	header3.checksum = 0;
	header3.checksum = checksum(header3);  // ����У���  
	memcpy(buf3, &header3, sizeof(header3));  // ���뻺����
	tag = sendto(send_socket, buf3, sizeof(header3), 0, (sockaddr*)&recv_addr, recv_addrlen);
	if (tag == -1) {  //��֤���ͽ��
		cout << "���������ַ���ʧ�ܣ�" << endl;
		return false;
	}
	else {
		cout << "���������ֳɹ�����" << endl;
	}

	// �ָ�����ģʽ
	mode = 0;
	ioctlsocket(send_socket, FIONBIO, &mode);
	// �������ֽ������ɹ�����
	cout << "---------- �������ֳɹ������ӳɹ��� ----------" << endl;
	return true;
}

// �Ĵλ��ֶϿ�����
bool DisConnect(SOCKET& send_socket, SOCKADDR_IN& recv_addr, int& recv_addrlen) {
	cout << "---------- ��ʼ���ֶϿ� ----------" << endl;
	// ��socket��Ϊ������״̬��ʹ��������ִ�У����жϳ�ʱ�ش�
	u_long mode = 1;
	ioctlsocket(send_socket, FIONBIO, &mode);

	// ��һ�λ��֣�����FIN+ACK
	HEADER header1;
	char* buf1 = new char[sizeof(header1)];
	header1.flag = FIN_ACK;  // ����flag
	header1.checksum = 0;
	header1.checksum = checksum(header1);  // ����У��� 
	memcpy(buf1, &header1, sizeof(header1));  // ���뻺����
	int tag = sendto(send_socket, buf1, sizeof(header1), 0, (sockaddr*)&recv_addr, recv_addrlen);
	if (tag == -1) {  //��֤���ͽ��
		cout << "��һ�λ��ַ���ʧ�ܣ�" << endl;
		return false;
	}
	else {
		cout << "��һ�λ��ֳɹ����� " << endl;
	}
	clock_t start = clock();  // ��¼��һ�λ���ʱ��

	// �ڶ��λ��֣��ȴ�ACK
	HEADER header2;
	char* buf2 = new char[sizeof(header2)];
	while (true) {
		int recv_lenth = recvfrom(send_socket, buf2, sizeof(header2), 0, (sockaddr*)&recv_addr, &recv_addrlen);

		// ���յ������flag��У���
		if (recv_lenth > 0) {
			memcpy(&header2, buf2, sizeof(header2));
			if (header2.flag == ACK && checksum(header2) == 0) {
				cout << "�յ��ڶ��λ���" << endl;
				break;
			}
			else {
				cout << "�ڶ��λ������ݰ�����" << endl;
			}
		}

		// ÿ��ѭ���ж��Ƿ�ʱ
		if (clock() - start > MAX_TIME) {
			cout << "�ȴ��ڶ��λ��ֳ�ʱ���ش���һ�λ���" << endl;
			tag = sendto(send_socket, buf1, sizeof(header1), 0, (sockaddr*)&recv_addr, recv_addrlen);
			if (tag == -1) {  //��֤���ͽ��
				cout << "��һ�λ����ش�ʧ�ܣ�" << endl;
				return false;
			}
			else {
				cout << "��һ�λ����Ѿ��ش� " << endl;
			}
			start = clock();  // ���»���ʱ��
		}
	}

	// �����λ��֣��ȴ�FIN+ACK
	HEADER header3;
	char* buf3 = new char[sizeof(header3)];
	start = clock();  // ���»���ʱ��
	while (true) {
		int recvlength = recvfrom(send_socket, buf3, sizeof(header3), 0, (sockaddr*)&recv_addr, &recv_addrlen);

		// ���յ������flag��У���
		if (recvlength > 0) {
			memcpy(&header3, buf3, sizeof(header3));
			if (header3.flag == FIN_ACK && checksum(header3) == 0) {
				cout << "�յ������λ���" << endl;
				break;
			}
			else {
				cout << "�����λ������ݰ�����" << endl;
			}
		}
	}

	// ���Ĵλ��֣�����ACK
	HEADER header4;
	char* buf4 = new char[sizeof(header4)];
	header4.flag = ACK;  // ����flag
	header4.checksum = 0;
	header4.checksum = checksum(header4);  // ����У��� 
	memcpy(buf4, &header4, sizeof(header4));  // ���뻺����
	tag = sendto(send_socket, buf4, sizeof(header4), 0, (sockaddr*)&recv_addr, recv_addrlen);
	if (tag == -1) {  //��֤���ͽ��
		cout << "���Ĵλ��ַ���ʧ�ܣ�" << endl;
		return false;
	}
	else {
		cout << "���Ĵλ��ֳɹ����� " << endl;
	}

	// �ָ�����ģʽ
	mode = 0;
	ioctlsocket(send_socket, FIONBIO, &mode);
	cout << "---------- �Ĵλ��ֳɹ����Ͽ����ӳɹ���----------" << endl;
	return true;
}

mutex ack_mutex;  // ����ͬ��ACK���յ��߳������߳�
int head = 0;     // ����ͷ��
int tail = 0;     // ����β��
int now = 0;      // ��ǰ���͵���λ��

// ����ACK�̺߳���
void ReceiveACK(SOCKET& send_socket, SOCKADDR_IN& recv_addr, int& recv_addrlen, int& head, int& tail, int times) {
	HEADER header2;
	char* buf2 = new char[sizeof(header2)];
	
	while (head < times) {
		int recvlength = recvfrom(send_socket, buf2, sizeof(header2), 0, (sockaddr*)&recv_addr, &recv_addrlen);

		if (recvlength > 0) {  // �յ�
			memcpy(&header2, buf2, sizeof(header2));
			// ���У���
			if (checksum(header2) != 0) {
				cout << "У��ʹ���" << endl;
				continue;
			}
			else {
				lock_guard<mutex> lock(ack_mutex);  // ��סACK���ղ��֣���֤�̰߳�ȫ
				
				if ((int)header2.ack == (head % 256)) {  // �յ��ظ�ACK������
					continue;
				}
				else {  // ��ȷACK�����´��� 
					int step = ((int)header2.ack - (head % 256) + 256) % 256;
					cout << "[����] �յ���ȷack��" << (int)header2.ack << "  ���ڸ��£�����" << step << endl;

					head += step;
					tail = (tail + step > times) ? times : (tail + step);
				}
			}
		}
	}
}

// ���ݴ���
bool Send(SOCKET& send_socket, SOCKADDR_IN& recv_addr, int& recv_addrlen, char* message, int msg_len) {
	cout << "---- �����ļ����亯�� ----" << endl;
	
	int times = msg_len / MAXSIZE + (msg_len % MAXSIZE != 0);  // ��Ҫ���͵Ĵ���������ȡ��

	cout << "�ļ���С " << msg_len << " �ֽ�" << "��Ҫ���� " << times << " ��" << endl;

	tail = (times > WINDOWS) ? WINDOWS : times;  // β��
	clock_t start = 0;  // ��¼���ڷ���ʱ��

	// ��socket��Ϊ������״̬��ʹ��������ִ�У����жϳ�ʱ�ش�
	u_long mode = 1;
	ioctlsocket(send_socket, FIONBIO, &mode);

	// ��������ACK���߳�
	thread ack_thread(ReceiveACK, ref(send_socket), ref(recv_addr), ref(recv_addrlen), ref(head), ref(tail), times);
	
	// �������ƣ�ֱ������
	while (head < times) {
		lock_guard<mutex> lock(ack_mutex);  // ��ס���̣߳����ⴰ�ڸ��³�ͻ
		if (now < tail) {  // ��Ҫ���з���
			// ������ǰ���ڣ�ѭ������head��tail������
			//lock_guard<mutex> lock(ack_mutex);  // ��ס���̣߳����ⴰ�ڸ��³�ͻ
			for (now; now < tail; now++) {
				cout << "[����] ���ʹ���:" << head << " -- " << tail << "  now: " << now << "  seq: " << now % 256 << endl;

				// ���㱾�δ������ݴ�С
				int len = (now == times - 1) ? (msg_len - MAXSIZE * (times - 1)) : MAXSIZE;

				// ����header
				HEADER header1;
				char* buf1 = new char[sizeof(header1) + len];
				header1.flag = 0;
				header1.length = len;
				header1.seq = now % 256;
				header1.checksum = 0;
				header1.checksum = checksum(header1);
				memcpy(buf1, &header1, sizeof(header1));

				// ������Ϣ
				char* msg = &message[now * MAXSIZE];
				memcpy(buf1 + sizeof(header1), msg, len);

				// ����

				sendto(send_socket, buf1, sizeof(header1) + len, 0, (sockaddr*)&recv_addr, recv_addrlen);
				start = clock();  // ��¼���ڷ���ʱ��
			}
			
		}
		else if (clock() - start > MAX_TIME) {  // ����Ҫ���ͣ��жϳ�ʱ
			// lock_guard<mutex> lock(ack_mutex);
			cout << "�ȴ�����ACK��ʱ���ش�" << endl;
			now = head;
			start = clock();
		}
	}

	cout << "�ļ�������ȫ������" << endl;

	ack_thread.join();  // �ȴ��̴߳�����ȫ��ACK

	// ���ͽ�����־
	HEADER header3;
	char* buf3 = new char[sizeof(header3)];
	header3.flag = OVER;  // ����flag
	header3.checksum = 0;
	header3.checksum = checksum(header3);  // ����У��� 
	memcpy(buf3, &header3, sizeof(header3));  // ���뻺����
	
	sendto(send_socket, buf3, sizeof(header3), 0, (sockaddr*)&recv_addr, recv_addrlen);
	
	start = clock();  // ��¼ʱ��

	// �ȴ�������־ȷ��
	while (true) {
		int recvlength = recvfrom(send_socket, buf3, sizeof(header3), 0, (sockaddr*)&recv_addr, &recv_addrlen);

		// ���յ�������־λ��У���
		if (recvlength > 0) {
			memcpy(&header3, buf3, sizeof(header3));
			if (header3.flag == OVER && checksum(header3) == 0) {
				cout << "�յ�������־ACK" << endl;
				break;
			}
			else {
				cout << "������־ACK����" << endl;
			}
		}

		if (clock() - start > MAX_TIME) {
			cout << "�ȴ�������־ȷ�ϳ�ʱ���ش�" << endl;
			sendto(send_socket, buf3, sizeof(header3), 0, (sockaddr*)&recv_addr, recv_addrlen);
			start = clock();  // ��¼ʱ��
		}
	}

	// �ָ�����ģʽ
	mode = 0;
	ioctlsocket(send_socket, FIONBIO, &mode);
	// �ļ��������
	cout << "---- �ļ����亯������ ----" << endl;
	return true;
}

int main() {
	cout << "����ش�ʱ�䣺" << MAX_TIME << endl;

	WSADATA wsadata;
	WSAStartup(MAKEWORD(2, 2), &wsadata);

	struct sockaddr_in recv_addr;
	recv_addr.sin_family = AF_INET;  // IPV4
	recv_addr.sin_port = htons(SendPort);
	inet_pton(AF_INET, "127.0.0.1", &recv_addr.sin_addr.s_addr);

	SOCKET send = socket(AF_INET, SOCK_DGRAM, 0);

	int len = sizeof(recv_addr);

	// ������������
	if (Connect(send, recv_addr, len) == false) {
		cout << "����ʧ�ܣ�!" << endl;
		return 0;
	}

	bool flag;
	cout << "����������룺\n" << "0���˳�  " << "1�������ļ�" << endl;
	cin >> flag;
	if (flag == true) {  // �����ļ�
		cout << "��������Ҫ������ļ�����";
		string filename;
		cin >> filename;
		cout << "�����봰�ڴ�С��";
		cin >> WINDOWS;

		ifstream fileIN(filename.c_str(), ifstream::binary); // �Զ����Ʒ�ʽ���ļ�
		// �����ļ�����
		char* buf = new char[100000000];
		int i = 0;
		unsigned char temp = fileIN.get();
		while (fileIN)
		{
			buf[i++] = temp;
			temp = fileIN.get();
		}
		fileIN.close();

		// ��ʼ����
		cout << "---------- ��ʼ�����ļ� ----------" << endl;
		clock_t begin = clock(); // ��ʼʱ��

		// �����ļ���
		cout << "�ļ������䣺" << endl;
		head = 0;     // �ѷ���δȷ�ϵ�ͷ��
		tail = 0;     // ��ǰ����β��
		now = 0;      // ��ǰ���͵����к�
		Send(send, recv_addr, len, (char*)(filename.c_str()), filename.length());
		// �����ļ�����
		cout << "�ļ����ݴ��䣺" << endl;
		head = 0; 
		tail = 0; 
		now = 0; 
		Send(send, recv_addr, len, buf, i);

		clock_t end = clock(); // ����ʱ��

		// ����ʱ���������λΪ��
		double elapsed_time = static_cast<double>(end - begin) / CLOCKS_PER_SEC;
		cout << "�ļ��������ĵ�ʱ��: " << elapsed_time << " ��" << endl;
		// ����������
		double throughput = (i + filename.length()) / elapsed_time;
		cout << "�ļ�����������: " << throughput << " byte/s" << endl;
		cout << "---------- �ļ�������� ----------" << endl;

	}
	else {  // ���������ļ�������־
		u_long mode = 1;
		ioctlsocket(send, FIONBIO, &mode);

		HEADER header1;
		char* buf1 = new char[sizeof(header1)];
		header1.flag = OVER;  // ����flag
		header1.checksum = 0;
		header1.checksum = checksum(header1);  // ����У��� 
		memcpy(buf1, &header1, sizeof(header1));  // ���뻺����

		//// ������־1
		sendto(send, buf1, sizeof(header1), 0, (sockaddr*)&recv_addr, len);
		cout << "������־1���ͳɹ�" << endl;
		clock_t start = clock();  // ��¼ʱ��
		// �ȴ�������־ȷ��
		HEADER header2;
		char* buf2 = new char[sizeof(header2)];
		while (true) {
			int recvlength = recvfrom(send, buf2, sizeof(header2), 0, (sockaddr*)&recv_addr, &len);

			// ���յ�������־λ��У���
			if (recvlength > 0) {
				memcpy(&header2, buf2, sizeof(header2));
				if (header2.flag == OVER && checksum(header2) == 0) {
					cout << "�յ�������־1ACK" << endl;
					break;
				}
				else {
					cout << "������־1ACK����" << endl;
				}
			}

			if (clock() - start > MAX_TIME) {
				cout << "�ȴ�������־1ȷ�ϳ�ʱ���ش�һ��" << endl;
				sendto(send, buf1, sizeof(header1), 0, (sockaddr*)&recv_addr, len);
				cout << "������־1�ش��ɹ�" << endl;
				start = clock();  // ��¼ʱ��
			}
		}

		//// ������־2
		sendto(send, buf1, sizeof(header1), 0, (sockaddr*)&recv_addr, len);
		cout << "������־2���ͳɹ�" << endl;
		start = clock();  // ��¼ʱ��
		// �ȴ�������־ȷ��
		while (true) {
			int recvlength = recvfrom(send, buf2, sizeof(header2), 0, (sockaddr*)&recv_addr, &len);

			// ���յ�������־λ��У���
			if (recvlength > 0) {
				memcpy(&header2, buf2, sizeof(header2));
				if (header2.flag == OVER && checksum(header2) == 0) {
					cout << "�յ�������־2ACK" << endl;
					break;
				}
				else {
					cout << "������־2ACK����" << endl;
				}

			}
			if (clock() - start > MAX_TIME) {
				cout << "�ȴ�������־2ȷ�ϳ�ʱ���ش�һ��" << endl;
				sendto(send, buf1, sizeof(header1), 0, (sockaddr*)&recv_addr, len);
				cout << "������־2�ش��ɹ�" << endl;
				start = clock();  // ��¼ʱ��
			}
		}

		// �ָ�����ģʽ
		mode = 0;
		ioctlsocket(send, FIONBIO, &mode);
	}

	// �Ĵλ��ֶϿ�����
	if (DisConnect(send, recv_addr, len) == false) {
		cout << "�Ͽ�����ʧ�ܣ�!" << endl;
		return 0;
	}

	system("pause");
	return 0;
}