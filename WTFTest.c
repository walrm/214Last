#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 

int main(int argc, char *argv[]){
    
    chdir("./Client");
    system("./WTF Configure localhost 1234");
    int x=system("diff .configure ../testCases/.configure");
    if(x){
        printf("Error in test 1\n");
        return 1;
    } 
    else{
        printf("Test 1 Passed\n");
    }

    system("cp -a ../testCases/WTFTest/ ../Server/");
    system("./WTF Checkout WTFTest");
    system("rm ./WTFTest/file1");
    system("./WTF Remove WTFTest file1");
    system("echo \"added file\">WTFTest/file6");
    system("./WTF Add WTFTest file6");
    printf("Here\n");
    system("./WTF Commit WTFTest");
    system("./WTF Push WTFTest");
    x=system("diff -r WTFTest ../testCases/test2/test2Client");
    int y=system("diff -r -x *.tar.gz ../Server/WTFTest ../testCases/test2/test2Server");
    if(x||y){
        printf("Error in test 2,%d,%d\n",x,y);
        return 1;
    }
    else{
        printf("Test 1 Passed\n");
    }





/*
./WTF Configure localhost 1234
./WTF Create test1
echo “test123 test123” >test1/file1.txt
Echo “file2” >test1/file2.txt
./WTF Add test1 file1.txt
./WTF Add test1 file2.txt
./WTF Commit test1
./WTF Push test1
diff -r test1 ../testCases/Client/test1
Diff -r ../Server/test1 ../testCases/Server/test1
rm test1
*/
    return 1;
}
