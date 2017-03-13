#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>


#pragma pack(push,1)
struct BootEntry {
    unsigned char BS_jmpBoot[3];    /* Assembly instruction to jump to boot code */
    unsigned char BS_OEMName[8];    /* OEM Name in ASCII */
    unsigned short BPB_BytsPerSec;  /* Bytes per sector. Allowed values include 512,1024, 2048, and 4096 */
    unsigned char BPB_SecPerClus;   /* Sectors per cluster (data unit). Allowed values are powers of 2, but the cluster size must be 32KB or smaller */
    unsigned short BPB_RsvdSecCnt;  /* Size in sectors of the reserved area */
    unsigned char BPB_NumFATs;      /* Number of FATs */
    unsigned short BPB_RootEntCnt;  /* Maximum number of files in the root directory for FAT12 and FAT16. This is 0 for FAT32 */
    unsigned short BPB_TotSec16;    /* 16-bit value of number of sectors in file system */
    unsigned char BPB_Media;        /* Media type */
    unsigned short BPB_FATSz16;     /* 16-bit size in sectors of each FAT for FAT12 and FAT16. For FAT32, this field is 0 */
    unsigned short BPB_SecPerTrk;   /* Sectors per track of storage device */
    unsigned short BPB_NumHeads;    /* Number of heads in storage device */
    unsigned long BPB_HiddSec;      /* Number of sectors before the start of partition */
    unsigned long BPB_TotSec32;     /* 32-bit value of number of sectors in file system. */
    unsigned long BPB_FATSz32;      /* 32-bit size in sectors of one FAT */
    unsigned short BPB_ExtFlags;    /* A flag for FAT */
    unsigned short BPB_FSVer;       /* The major and minor version number */
    unsigned long BPB_RootClus;     /* Cluster where the root directory can be found */
    unsigned short BPB_FSInfo;      /* Sector where FSINFO structure can be found */
    unsigned short BPB_BkBootSec;   /* Sector where backup copy of boot sector is located */
    unsigned char BPB_Reserved[12]; /* Reserved */
    unsigned char BS_DrvNum;        /* BIOS INT13h drive number */
    unsigned char BS_Reserved1;     /* Not used */
    unsigned char BS_BootSig;       /* Extended boot signature to identify if the next three values are valid */
    unsigned long BS_VolID;         /* Volume serial number */
    unsigned char BS_VolLab[11];    /* Volume label in ASCII. User defines when creating the file system */
    unsigned char BS_FilSysType[8]; /* File system type label in ASCII */
} BootSector;
#pragma pack(pop)
#pragma pack(push,1)
struct DirEntry
{
    unsigned char DIR_Name[11];		/* File name */
    unsigned char DIR_Attr;         /* File attributes */
    unsigned char DIR_NTRes;		/* Reserved */
    unsigned char DIR_CrtTimeTenth;	/* Created time (tenths of second) */
    unsigned short DIR_CrtTime;		/* Created time (hours, minutes, seconds) */
    unsigned short DIR_CrtDate;		/* Created day */
    unsigned short DIR_LstAccDate;	/* Accessed day */
    unsigned short DIR_FstClusHI;	/* High 2 bytes of the first cluster address */
    unsigned short DIR_WrtTime;		/* Written time (hours, minutes, seconds */
    unsigned short DIR_WrtDate;		/* Written day */
    unsigned short DIR_FstClusLO;	/* Low 2 bytes of the first cluster address */
    unsigned long DIR_FileSize;		/* File size in bytes. (0 for directories) */
} Entry;
#pragma pack(pop)

/* function */
void list_directory_entry(char* dir_path);
int recover_file(char* path_name);
char* output_filename(struct DirEntry Entry);
void fileOutput(char* Filename, int cluster, int fileSize);



/* Share instance */
FILE* file;
char* DEVICE_PATH;
unsigned int ROOT_START ;
unsigned int FAT_START;
int deleted_file_size = 0;
char* dir_path;
char* recover_path;
char* output_path;

int main(int argc, char *argv[]) {
    int opt;
    if(argc>3 && argc%2==1 && (strcmp(argv[1],"-d") == 0) && (strcmp(argv[3],"-l")==0 || (argc ==7 &&strcmp(argv[3],"-r")==0 && strcmp(argv[5],"-o")==0))){
        opt=0;
    }else{
        fprintf(stdout, "Usage: %s -d [device filename] [other arguments]\n", argv[0]);
        fprintf(stdout, "-l target            List the target directory\n");
        fprintf(stdout, "-r target -o dest    Recover the target pathname\n");
        return 0;
        exit(0);
    }
    while ((opt = getopt(argc, argv, "d:l:r:o:")) != -1) {
        switch (opt) {
            case 'd':
                DEVICE_PATH = optarg;
                file = fopen(DEVICE_PATH, "rb");
                fread(&BootSector, sizeof(BootSector), 1, file);
                break;
            case 'l':
                dir_path = optarg;
                list_directory_entry(dir_path);
                break;
            case 'r':
                recover_path = optarg;
                int clusterNeedRecover = recover_file(recover_path);
                //printf("the clusterNeedRecover = %d\n", clusterNeedRecover);
                break;
            case 'o':
                output_path = optarg;
                if(clusterNeedRecover != 0)
                    fileOutput(output_path, clusterNeedRecover, deleted_file_size);
                
                break;
                
        }
    }
    
    fclose(file);
    return 0;
}

int recover_file(char* pathname){
    int byte_count = 0;
    unsigned int current_cluster_start = 0;
    int current_cluster = 2;
    int next_cluster = 0;
    char* pathname2 = (char*)malloc(sizeof(char)*strlen(pathname));
    char theFirstLetter;
    strcpy(pathname2, pathname);
    char* path_token = strtok(pathname2, "/");
    ROOT_START = (BootSector.BPB_RsvdSecCnt + BootSector.BPB_FATSz32 * BootSector.BPB_NumFATs) * BootSector.BPB_BytsPerSec;
    FAT_START = BootSector.BPB_RsvdSecCnt * BootSector.BPB_BytsPerSec;
    
    while (path_token != NULL){
        current_cluster_start = ROOT_START + (current_cluster - BootSector.BPB_RootClus) * BootSector.BPB_SecPerClus * BootSector.BPB_BytsPerSec;
        fseek(file, current_cluster_start, SEEK_SET);
        while (1) {
            byte_count += 32;
            fread(&Entry, sizeof(Entry), 1, file);
            int DIRmatch = 0;
            int fileMatch = 0;
            int i=0;
            char* dot = strstr(path_token, ".");
            //int IsFile = 0;
            // match the filename
            // printf("This is %d", i++);
            char* StdFileName = output_filename(Entry);
            if(dot == NULL){ //for path_token is a directory
                //Find it is DIR or file
                if(Entry.DIR_Attr == 0x10){
                    if(strcmp(path_token, StdFileName)== 0){
                        DIRmatch = 1;
                    }
                }else{
                    path_token[0] = 63;
                    if(strcmp(path_token, StdFileName) == 0){
                        fileMatch = 1;
                    }
                }
            }else{//for path_token is a directory
                theFirstLetter = path_token[0];
                path_token[0] = 63;
                if(strcmp(path_token, StdFileName)== 0){
                    fileMatch = 1;
                }else{
                    fileMatch = 0;
                }
            }
            if(fileMatch == 1){
                //  printf("path_token matched, proceed......\n");
                //check FAT if the target cluster is not occupied
                int target_cluster = (Entry.DIR_FstClusHI<<8|Entry.DIR_FstClusLO);
                //  int FAT_target_cluster = FAT_START + target_cluster * 4;
                fseek(file, FAT_START + target_cluster * 4, SEEK_SET); // read the FAT[target_cluster]
                fread(&target_cluster, 4, 1, file);
                target_cluster &= 0x0FFFFFFF;
                
                if(target_cluster == 0){// create a new file and return
                    target_cluster = (Entry.DIR_FstClusHI<<8|Entry.DIR_FstClusLO);
                    //  printf("The target_cluster = %d\n", target_cluster);
                    deleted_file_size = Entry.DIR_FileSize;
                    //  printf("The deleted_file_size = %d\n", deleted_file_size);
                    return target_cluster;
                    
                    
                }else{
                    printf("[%s]: error - fail to recover\n", pathname);
                    return 0;
                }
                
            }
            
            if (Entry.DIR_Name[0] == 0x00 || byte_count >= (BootSector.BPB_SecPerClus * BootSector.BPB_BytsPerSec)&& DIRmatch == 0 && fileMatch == 0) {
                //find next cluster
                
                fseek(file ,FAT_START+current_cluster * 4, SEEK_SET);
                fread(&current_cluster, 4, 1, file);
                current_cluster &= 0x0FFFFFFF;
                
                if(current_cluster != 0x0FFFFFFF){
                    
                    current_cluster_start = ROOT_START + (current_cluster - BootSector.BPB_RootClus) * BootSector.BPB_SecPerClus * BootSector.BPB_BytsPerSec;
                    byte_count = 0;
                    fseek(file, current_cluster_start, SEEK_SET);
                    //find next cluster
                }else{
                    printf("[%s]: error - file not found\n", pathname );
                    return 0;
                }
            }
            
            if (Entry.DIR_Attr == 0x0f) {
                continue;
            }
            
            if (strcmp(path_token, output_filename(Entry))==0) {
                //  printf("Loading into the directory[ /%s ]...\n", path_token);
                current_cluster = (Entry.DIR_FstClusHI<<8|Entry.DIR_FstClusLO);
                break;
            }
            
        }
        path_token = strtok(NULL, "/");
        
    }
    
    
    return 0;
}

void fileOutput(char* Filename, int cluster, int fileSize){
    FILE * newFile;
    
    unsigned char *buff = (char*)malloc(fileSize);
    newFile = fopen(Filename, "wb+");
    if(newFile){
        //the file exist
        ROOT_START = (BootSector.BPB_RsvdSecCnt + BootSector.BPB_FATSz32 * BootSector.BPB_NumFATs) * BootSector.BPB_BytsPerSec;
        fseek(file, ROOT_START + (cluster - 2) * BootSector.BPB_SecPerClus * BootSector.BPB_BytsPerSec, SEEK_SET);
        //  printf("the data start = %d\n\n", ROOT_START + (cluster - 2) * BootSector.BPB_SecPerClus * BootSector.BPB_BytsPerSec);
        fread(buff, fileSize, 1, file);
        /*int i=0; for(i=0;i<17;i++)printf("%c ", buff[i]); printf("\n");*/ // this line is used to check the content of the deleted file
        rewind(newFile);
        fwrite(buff, fileSize, 1, newFile );
        printf("[%s]: recovered\n", recover_path);
        fclose(newFile);
        
    }else{
        printf("[%s]: failed to open\n", Filename);
    }
}



void list_directory_entry(char* dir_path) {
    int file_count = 0;
    int current_cluster = 2;
    int next_cluster = 0;
    unsigned int current_cluster_start;
    int bytes_count = 0;
    
    ROOT_START = (BootSector.BPB_RsvdSecCnt + BootSector.BPB_NumFATs * BootSector.BPB_FATSz32) * BootSector.BPB_BytsPerSec;
    FAT_START = BootSector.BPB_RsvdSecCnt * BootSector.BPB_BytsPerSec;
    current_cluster_start = ROOT_START + (current_cluster - BootSector.BPB_RootClus) * BootSector.BPB_SecPerClus * BootSector.BPB_BytsPerSec;
    
    char* path_token = strtok(dir_path, "/");
    
    while (path_token != NULL) {
        current_cluster_start = ROOT_START + (current_cluster - BootSector.BPB_RootClus) * BootSector.BPB_SecPerClus * BootSector.BPB_BytsPerSec;
        bytes_count = 0;
        fseek(file, current_cluster_start, SEEK_SET);
        while (1) {
            fread(&Entry, sizeof(Entry), 1, file);
            bytes_count += 32;
            if (Entry.DIR_Name[0] == 0x00) {
                break;
            }
            if (Entry.DIR_Attr == 0x0f) {
                continue;
            }
            
            if (strcmp(path_token, output_filename(Entry)) == 0) {
                current_cluster = (Entry.DIR_FstClusHI<<8|Entry.DIR_FstClusLO);
                current_cluster_start = ROOT_START + (current_cluster - BootSector.BPB_RootClus) * BootSector.BPB_SecPerClus * BootSector.BPB_BytsPerSec;
                break;
            }
            
            if (bytes_count >= (BootSector.BPB_SecPerClus * BootSector.BPB_BytsPerSec)) {
                fseek(file ,FAT_START+current_cluster * 4, SEEK_SET);
                fread(&current_cluster, 4, 1, file);
                current_cluster &= 0x0FFFFFFF;
                
                current_cluster_start = ROOT_START + (current_cluster - BootSector.BPB_RootClus) * BootSector.BPB_SecPerClus * BootSector.BPB_BytsPerSec;
                
                fseek(file, current_cluster_start, SEEK_SET);
                
                printf("new cluster : %d \n", current_cluster);
                bytes_count = 0;
            }
            
        }
        path_token = strtok(NULL, "/");
    }
    
    fseek(file, current_cluster_start, SEEK_SET);
    bytes_count = 0;
    while (1) {
        
        fread(&Entry, sizeof(Entry), 1, file);
        bytes_count += 32;
        
        if (Entry.DIR_Name[0] == 0x00) {
            break;
        }
        if (Entry.DIR_Attr == 0x0f) {
            continue;
        }
        
        printf("%d, %s", ++file_count, output_filename(Entry));
        if (Entry.DIR_Attr == 0x10) { printf("/"); }
        printf(", %lu, %d\n", Entry.DIR_FileSize, (Entry.DIR_FstClusHI<<8|Entry.DIR_FstClusLO));
        
        if (bytes_count >= (BootSector.BPB_SecPerClus * BootSector.BPB_BytsPerSec)) {
            fseek(file ,FAT_START+current_cluster * 4, SEEK_SET);
            fread(&current_cluster, 4, 1, file);
            current_cluster &= 0x0FFFFFFF;
            
            current_cluster_start = ROOT_START + (current_cluster - BootSector.BPB_RootClus) * BootSector.BPB_SecPerClus * BootSector.BPB_BytsPerSec;
            
            fseek(file, current_cluster_start, SEEK_SET);
            printf("new cluster : %d \n", current_cluster);
            bytes_count = 0;
        }
        
    }
    //printf("byte count %d \n", bytes_count);
    
}
char* output_filename(struct DirEntry Entry) {
    
    char* output_name = malloc(sizeof(char) * 12);
    int i, name_part_length = 0, ext_part_length = 0;
    
    for (i = 0; i < 11; i++) {
        if (Entry.DIR_Name[i] == 0xe5) {
            if (i < 8) {
                name_part_length++;
            } else {
                ext_part_length++;
            }
            
        } else if (Entry.DIR_Name[i] != 0x20) {
            if (i < 8) {
                name_part_length++;
            } else {
                ext_part_length++;
            }
        }
    }
    
    if (name_part_length > 0) {
        int i=0 ,j;
        for (j=0; j<8; j++) {
            if (Entry.DIR_Name[j] == 0xe5) {
                output_name[i]=63;
                i++;
            } else if (Entry.DIR_Name[j] != 0x20) {
                output_name[i] = Entry.DIR_Name[j];
                i++;
            }
        }
        if (ext_part_length > 0) {
            output_name[i] = '.';
            i++;
            for (j=8; j<11; j++) {
                if (Entry.DIR_Name[j] != 0x20) {
                    output_name[i] = Entry.DIR_Name[j];
                    i++;
                }
            }
        }
        output_name[i] = '\0';
    }
    
    return output_name;
    
}
