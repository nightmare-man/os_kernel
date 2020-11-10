#此工具用来看目录里的每一个文件 有无某一个字符串
import os
def get_file_path(root_path,tar_str):
    #获取该目录下所有的文件名称和目录名称
    dir_or_files = os.listdir(root_path)
    for dir_file in dir_or_files:
        #获取目录或者文件的路径
        dir_file_path = os.path.join(root_path,dir_file)
        #判断该路径为文件还是路径
        if os.path.isdir(dir_file_path):
            #递归获取所有文件和目录的路径
            get_file_path(dir_file_path,tar_str)
        else:
            try:
                f=open(dir_file_path,"r",encoding="utf-8")
                data=f.read()
                f.close()
                if(data.find(tar_str)!=-1):
                    print(dir_file_path)
            except:
                print("")
get_file_path("/home/lsm/os_kernel/","volatile")
