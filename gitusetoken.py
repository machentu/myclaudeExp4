import pyautogui
import time
import argparse

import sys 
sys.path.append("..") 
import gitconfig

parser = argparse.ArgumentParser(description='please input the commit command')
parser.add_argument('--cmd', type=str, default='git pull')
# parser.add_argument('--password', type=str)
parser.add_argument('--username', type=str, default='machentu')

args = parser.parse_args()

def enter_password(password):
    time.sleep(1)  # 等待5秒钟以切换到密码输入界面
    pyautogui.write(password)
    pyautogui.press('enter')

curcmd = args.cmd
username = args.username
password = gitconfig.password

print(curcmd)
# print(password)
enter_password(curcmd) 
enter_password(username)
enter_password(password)