/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "storage/disk_manager.h"

#include <assert.h>    // for assert
#include <string.h>    // for memset
#include <sys/stat.h>  // for stat
#include <unistd.h>    // for lseek

#include "defs.h"

DiskManager::DiskManager() { memset(fd2pageno_, 0, MAX_FD * (sizeof(std::atomic<page_id_t>) / sizeof(char))); }

/**
 * @description: ������д���ļ���ָ������ҳ����
 * @param {int} fd �����ļ����ļ����
 * @param {page_id_t} page_no д��Ŀ��ҳ���page_id
 * @param {char} *offset Ҫд����̵�����
 * @param {int} num_bytes Ҫд����̵����ݴ�С
 */
void DiskManager::write_page(int fd, page_id_t page_no, const char* offset, int num_bytes) {
    // Todo:
    // 1.lseek()��λ���ļ�ͷ��ͨ��(fd,page_no)���Զ�λָ��ҳ�漰���ڴ����ļ��е�ƫ����
    // 2.����write()����
    // ע��write����ֵ��num_bytes����ʱ throw InternalError("DiskManager::write_page Error");
    lseek(fd, page_no * PAGE_SIZE, SEEK_SET);
    int flag = write(fd, offset, num_bytes);

    if (flag != num_bytes) {
        throw InternalError("DiskManager::write_page Error");
    }
}

/**
 * @description: ��ȡ�ļ���ָ����ŵ�ҳ���еĲ������ݵ��ڴ���
 * @param {int} fd �����ļ����ļ����
 * @param {page_id_t} page_no ָ����ҳ����
 * @param {char} *offset ��ȡ������д�뵽offset��
 * @param {int} num_bytes ��ȡ����������С
 */
void DiskManager::read_page(int fd, page_id_t page_no, char* offset, int num_bytes) {
    // Todo:
    // 1.lseek()��λ���ļ�ͷ��ͨ��(fd,page_no)���Զ�λָ��ҳ�漰���ڴ����ļ��е�ƫ����
    // 2.����read()����
    // ע��read����ֵ��num_bytes����ʱ��throw InternalError("DiskManager::read_page Error");

    lseek(fd, page_no * PAGE_SIZE, SEEK_SET);
    int flag = read(fd, offset, num_bytes);

    if (flag != num_bytes) {
        throw InternalError("DiskManager::read_page Error");
    }
}

/**
 * @description: ����һ���µ�ҳ��
 * @return {page_id_t} �������ҳ��
 * @param {int} fd ָ���ļ����ļ����
 */
page_id_t DiskManager::allocate_page(int fd) {
    // �򵥵�����������ԣ�ָ���ļ���ҳ���ż�1
    assert(fd >= 0 && fd < MAX_FD);
    return fd2pageno_[fd]++;
}

void DiskManager::deallocate_page(__attribute__((unused)) page_id_t page_id) {}

bool DiskManager::is_dir(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

void DiskManager::create_dir(const std::string& path) {
    // Create a subdirectory
    std::string cmd = "mkdir " + path;
    if (system(cmd.c_str()) < 0) {  // ����һ����Ϊpath��Ŀ¼
        throw UnixError();
    }
}

void DiskManager::destroy_dir(const std::string& path) {
    std::string cmd = "rm -r " + path;
    if (system(cmd.c_str()) < 0) {
        throw UnixError();
    }
}

/**
 * @description: �ж�ָ��·���ļ��Ƿ����
 * @return {bool} ��ָ��·���ļ������򷵻�true
 * @param {string} &path ָ��·���ļ�
 */
bool DiskManager::is_file(const std::string& path) {
    // ��struct stat��ȡ�ļ���Ϣ
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

/**
 * @description: ���ڴ���ָ��·���ļ�
 * @return {*}
 * @param {string} &path
 */
void DiskManager::create_file(const std::string& path) {
    // Todo:
    // ����open()������ʹ��O_CREATģʽ
    // ע�ⲻ���ظ�������ͬ�ļ�

    // if(this->is_file(path)){
    //     throw FileNotFoundError(path);
    // }
    // if(path2fd_.count(path)){
    //     throw UnixError();
    // }

    int fd = open(path.c_str(), O_CREAT | O_RDWR, 0666);
    // path.c_str()��std::stringת��Ϊconst char*��
    // ����O_CREAT��־λ���Դ����ļ������O_RDWR��־λ�����дȨ�ޣ������ļ�ģʽΪ0666��ʾ�κ��û����ж�дȨ�ޡ�

    if (fd == -1) {
        throw UnixError();
    }
    close(fd);
    return;
}

/**
 * @description: ɾ��ָ��·�����ļ�
 * @param {string} &path �ļ�����·��
 */
void DiskManager::destroy_file(const std::string& path) {
    // Todo:
    // ����unlink()����
    // ע�ⲻ��ɾ��δ�رյ��ļ�

    // �ж��ļ��Ƿ����
    if (!this->is_file(path)) {
        throw FileNotFoundError(path);
    }

    if (this->path2fd_.count(path)) {  // �Ѵ����ظ���
        throw FileNotClosedError(path);
    }
    // if(!close_file(path)){
    //     throw UnixError();
    // }

    if (unlink(path.c_str()) == -1) {
        throw InternalError("DiskManager::destroy_file Error: cannot delete file");
    }
}

/**
 * @description: ��ָ��·���ļ�
 * @return {int} ���ش򿪵��ļ����ļ����
 * @param {string} &path �ļ�����·��
 */
int DiskManager::open_file(const std::string& path) {
    // Todo:
    // ����open()������ʹ��O_RDWRģʽ
    // ע�ⲻ���ظ�����ͬ�ļ���������Ҫ�����ļ����б�

    if (!is_file(path)) {
        throw FileNotFoundError(path);
    }

    int fd = open(path.c_str(), O_RDWR);
    if (fd == -1) {
        throw InternalError("DiskManager::open_file Error: cannot open file");
    }
    // �����ļ����б�
    fd2path_[fd] = path;
    path2fd_[path] = fd;
    return fd;
}

/**
 * @description:���ڹر�ָ��·���ļ�
 * @param {int} fd �򿪵��ļ����ļ����
 */
void DiskManager::close_file(int fd) {
    // Todo:
    // ����close()����
    // ע�ⲻ�ܹر�δ�򿪵��ļ���������Ҫ�����ļ����б�
    // if(!fd2path_.count(fd)){
    //     throw FileNotOpenError(fd);
    //     return;//δ���򲻹ر�
    // }
    if (close(fd) == -1) {
        throw InternalError("DiskManager::close_file Error: cannot close file");
    }
    // �����ļ����б�
    if (fd2path_.count(fd)) {
        path2fd_.erase(fd2path_[fd]);
        fd2path_.erase(fd);
    }
    else {
        throw FileNotOpenError(fd);
        return;  // δ���򲻹ر�
    }
}

/**
 * @description: ����ļ��Ĵ�С
 * @return {int} �ļ��Ĵ�С
 * @param {string} &file_name �ļ���
 */
int DiskManager::get_file_size(const std::string& file_name) {
    struct stat stat_buf;
    int rc = stat(file_name.c_str(), &stat_buf);
    return rc == 0 ? stat_buf.st_size : -1;
}

/**
 * @description: �����ļ��������ļ���
 * @return {string} �ļ������Ӧ�ļ����ļ���
 * @param {int} fd �ļ����
 */
std::string DiskManager::get_file_name(int fd) {
    if (!fd2path_.count(fd)) {
        throw FileNotOpenError(fd);
    }
    return fd2path_[fd];
}

/**
 * @description:  ����ļ�����Ӧ���ļ����
 * @return {int} �ļ����
 * @param {string} &file_name �ļ���
 */
int DiskManager::get_file_fd(const std::string& file_name) {
    if (!path2fd_.count(file_name)) {
        return open_file(file_name);
    }
    return path2fd_[file_name];
}

/**
 * @description:  ��ȡ��־�ļ�����
 * @return {int} ���ض�ȡ������������Ϊ-1˵����ȡ���ݵ���ʼλ�ó������ļ���С
 * @param {char} *log_data ��ȡ���ݵ�log_data��
 * @param {int} size ��ȡ����������С
 * @param {int} offset ��ȡ���������ļ��е�λ��
 */
int DiskManager::read_log(char* log_data, int size, int offset) {
    // read log file from the previous end
    if (log_fd_ == -1) {
        log_fd_ = open_file(LOG_FILE_NAME);
    }
    int file_size = get_file_size(LOG_FILE_NAME);
    if (offset > file_size) {
        return -1;
    }

    size = std::min(size, file_size - offset);
    if (size == 0) return 0;
    lseek(log_fd_, offset, SEEK_SET);
    ssize_t bytes_read = read(log_fd_, log_data, size);
    assert(bytes_read == size);
    return bytes_read;
}

/**
 * @description: д��־����
 * @param {char} *log_data Ҫд�����־����
 * @param {int} size Ҫд������ݴ�С
 */
void DiskManager::write_log(char* log_data, int size) {
    if (log_fd_ == -1) {
        log_fd_ = open_file(LOG_FILE_NAME);
    }

    // write from the file_end
    lseek(log_fd_, 0, SEEK_END);
    ssize_t bytes_write = write(log_fd_, log_data, size);
    if (bytes_write != size) {
        throw UnixError();
    }
}
