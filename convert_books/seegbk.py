# 使用 encoding='gbk' 参数读取文件

filename = '《狼图腾》全集.txt'
with open(filename, 'r', encoding='gbk') as file:
    content = file.read()
    print(content[:100])

