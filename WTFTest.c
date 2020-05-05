#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[]){
    
    
    int pid = fork();
    if(pid == 0){
        chdir("./Server");
        system("./WTFserver 12345");
        return 0;
    }else{
        chdir("./Client");
        
        //Test case 1 - Configure
        system("./WTF Configure localhost 12345");
        int x=system("diff .configure ../testCases/.configure");
        if(x){
            printf("Error in test 1\n");
            return 1;
        } 
        else{
            printf("\nTest 1 Passed\n");
        }

        //Test case 2 - commit/push
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
            printf("\nTest 2 Passed\n\n");
        }
        system("rm -r ./WTFTest ../Server/WTFTest");

        //Test case 3 for upgrade/update
        system("./WTF Create WTFTest");
        system("cp -a ../testCases/WTFTest ../Server");
        system("./WTF Update WTFTest");
        system("./WTF Upgrade WTFTest");
        if(system("diff -r ../testCases/test3/test3Client ./WTFTest") == 0){
            printf("\nTest 3 passed. Update/Upgrade\n");
        }else{
            printf("\nERROR IN TEST 3\n\n");
        }
        system("rm -r ./WTFTest ../Server/WTFTest");

        //Test case 4 for Checkout
        system("cp -a ../testCases/WTFTest ../Server");
        system("./WTF Checkout WTFTest");
        if(system("diff -r ../testCases/test4/test4client ./WTFTest") == 0){
            printf("\nTest 4 passed. Checkout\n");
        }else{
            printf("\nERROR IN TEST 4\n");
        }
        system("rm -r ./WTFTest ../Server/WTFTest");

        //Test case 5 for rollback
        system("cp -a ../testCases/WTFTest/ ../Server/");
        system("./WTF Checkout WTFTest");
        system("echo \"test123 test123\" > ./WTFTest/file1.txt");
        system("echo \"file2\" > ./WTFTest/file2.txt");
        system("./WTF Add WTFTest file1.txt");
        system("./WTF Add WTFTest file2.txt");
        system("./WTF Commit WTFTest");
        system("./WTF Push WTFTest");
        system("./WTF Rollback WTFTest 2");
        if(system("diff -r ../testCases/test5/test5client ./WTFTest") == 0 && system("diff -r -x *.tar.gz ../testCases/test5/test5server ../Server/WTFTest") == 0){
            printf("\nTest 5 passed. Rollback\n");
        }else{
            printf("\nERROR IN TEST 5\n");
        }
        system("rm -r ./WTFTest ../Server/WTFTest");

        return 0;
    }
}