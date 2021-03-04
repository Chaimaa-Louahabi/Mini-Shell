#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include "readcmd.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>


enum etat {
    BG, FG, SUSPENDU
};
typedef enum etat etat_t;

typedef struct {
    int job_id;
    pid_t job_pid;
    etat_t job_etat;
    char *ligne_cmd;
} job_t ;

typedef struct {
    int N ; //nombre de fils créés(actifs et suspendus)
    int max; //max des id
    job_t list[];
} list_jobs;

/*Déclaration de liste_jobs*/
list_jobs liste_jobs;

/* Declaration du pid du processus en avant-plan */
pid_t pid_fg;

/*trouver l'indice de job dans list_jobs à partir de son pid*/
int get_job_indice(pid_t pid) {
    for (int i=0; i<= liste_jobs.N; i++) {
        if (liste_jobs.list[i].job_pid == pid) {
            return i;   
        }
    }
    return -1;
}

/*trouver le pid d'un job dans list_jobs à partir de son id*/
int get_pid(int id) {
    for (int i=0; i< liste_jobs.N; i++) {
        if (liste_jobs.list[i].job_id == id) {
            return liste_jobs.list[i].job_pid;   
        }
    }
    return -1;
}
/* Enregistrer le processus fils dans liste_jobs. */
void add_job(int pid_fils, char* cmd, etat_t etat) {
    liste_jobs.list[liste_jobs.N].job_id = liste_jobs.max +1;
    liste_jobs.list[liste_jobs.N].job_pid = pid_fils;
    /* enregistrer le nom de la première commande elementaire */
    char *ligne_cmde = malloc(10*sizeof(char));
    strcpy(ligne_cmde, cmd);
    liste_jobs.list[liste_jobs.N].ligne_cmd = ligne_cmde;
    liste_jobs.list[liste_jobs.N].job_etat = etat;
    liste_jobs.N++ ;
    liste_jobs.max ++;
}

int chercher_max_id() {
    int max = 0;
    for (int i= 0; i<= liste_jobs.N; i++) {
        if (liste_jobs.list[i].job_id > max) {
             max = liste_jobs.list[i].job_id ;
        }
    }
    return max;
}
/* supprimer un job de list_jobs à partir de son pid*/
void delete_job(pid_t pid) {
    int start = get_job_indice(pid);
    if (start != -1) {
        for (int i= start; i<= liste_jobs.N; i++) {
            liste_jobs.list[i] = liste_jobs.list[i + 1];
        }
        liste_jobs.N--;
        liste_jobs.max = chercher_max_id();
   }
}

/*Afficher un job*/
void print_job(job_t job){
    if (job.job_etat == BG){
        printf("[%d]     pid: %d     Actif(BG)    %s \n" ,job.job_id ,job.job_pid, job.ligne_cmd);
        fflush(stdout);
    } else if (job.job_etat == FG){
        printf("[%d]     pid: %d     Actif(FG)     %s \n" ,job.job_id ,job.job_pid, job.ligne_cmd);
        fflush(stdout);
    } else {
        printf("[%d]     pid: %d     Suspendu     %s \n" ,job.job_id ,job.job_pid, job.ligne_cmd);
        fflush(stdout);
    }
}

/* lister tous les processus actif/suspendu (la commande 'list')*/
void lister_jobs() {
    if (liste_jobs.N >= 1) {

        for (int i =0; i< liste_jobs.N; i++) {
            print_job(liste_jobs.list[i]);
        }
    }
}

/*La commande 'cd' */
void change_directory(char** cmd) {
    if ( cmd[1] == 0 || !strcmp(cmd[1], "~")) {

        chdir(getenv("HOME"));

    } else if (chdir(cmd[1])==-1) {

        printf(" %s : Aucun fichier ou dossier de ce type.\n", cmd[1]);
    }
}

/* la commande 'stop' */
void stop_processus ( char ** cmd) {
    //La saisie de "stop   " uniquement.
    if (cmd[1] == 0) {

        printf("Stop : utilisation : stop [identifiant du processus >=1] .\n");
    //La saisie d'un ID inexistant.
    } else if (atoi(cmd[1]) >liste_jobs.max | get_pid(atoi(cmd[1])) == -1 ){

        printf("stop : identifiant inexistant.\n");

    } else {
        int pid = get_pid(atoi(cmd[1]));
        kill(pid, SIGSTOP);
        liste_jobs.list[get_job_indice(pid)].job_etat = SUSPENDU;
    }
}

/* la commande 'bg' */
void background(char** cmd) {

    //La saisie de "bg  ".
    if (cmd[1] == 0) {

        printf("bg : utilisation : bg [identifiant du processus >=1] .\n");
    //La saisie d'un ID inexistant.
    } else if (get_pid(atoi(cmd[1])) < 0){

        printf("bg : identifiant inexistant.\n");

    } else {
        int pid = get_pid(atoi(cmd[1]));  
        kill(pid, SIGCONT);
        /* traiter la reprise */
        liste_jobs.list[get_job_indice(pid)].job_etat = BG;
    }
}

/*la commande 'fg' */
void foreground(char** cmd) {
    //La saisie de "fg  "
    if (cmd[1] == 0) {

        printf("fg : utilisation : fg [identifiant du processus >=1] .\n");
    //La saisie d'un ID inexistant.
    } else if (get_pid(atoi(cmd[1])) < 0){

        printf("fg : identifiant inexistant.\n");

    } else {
        int temp;
        int pid = get_pid(atoi(cmd[1]));
        int i = get_job_indice(pid);
        liste_jobs.list[i].job_etat = FG;
        //Afficher la première commande élémentaire de la ligne de commande
        printf("%s \n",liste_jobs.list[i].ligne_cmd);
        kill(pid, SIGCONT);
        wait(&temp);
        //delete the job after its end
        delete_job(pid);
    }
}
/*redirection de la sortie standard*/
void rediriger_sortie(char* fichier) {
    int desc_fich_out, dup_desc;
    desc_fich_out = open (fichier, O_WRONLY| O_CREAT | O_TRUNC, 0640);

    if (desc_fich_out < 0) {
        perror(fichier);
        exit(1);
    }

    dup_desc = dup2(desc_fich_out, 1);

    if (dup_desc == -1) {
        printf("Erreur dup2 \n");
        exit(1);
    }
}

/*redirection de l'entrée standard*/
void rediriger_entree(char* fichier) {
    int desc_fich_in, dup_desc;
    desc_fich_in = open (fichier, O_RDONLY);
    if (desc_fich_in < 0) {
        perror(fichier);
        exit(1);
    }
    dup_desc = dup2(desc_fich_in, 0);
    if (dup_desc == -1) {
        printf("Erreur dup2 \n");
        exit(1);
    }
}

/*handler de SIGINT */
void sigint_handler(int sig) {
    
   kill(SIGINT, pid_fg);
   delete_job(pid_fg);
}



/*handler de SIGCHDL */
void suivi_fils (int sig) {

    int etat_fils, pid_fils;

    do {
        pid_fils = (int) waitpid(-1, &etat_fils, WNOHANG | WUNTRACED | WCONTINUED);
        if ((pid_fils == -1) && (errno != ECHILD)) {
            perror("waitpid");
            exit(EXIT_FAILURE);
        } else if (pid_fils > 0) {
            int i = get_job_indice(pid_fils);
            if (i!= -1) {
                 if (WIFEXITED(etat_fils)) {
                    /* traiter exit */
                    delete_job(pid_fils);
                }
            }
        }

    } while (pid_fils > 0);
    return ;
}

/*******************************MAIN *******************************************/

/*******************************************************************************/

int main() {
    int wstatus, wstatus1, dup_desc, desc_fich_in, desc_fich_out, pid , pid2;
    liste_jobs.N = 0;
    //struct cmdline *ligneCommande;

    signal(SIGCHLD, suivi_fils);
    signal(SIGINT, sigint_handler);
    signal(SIGTSTP, SIG_IGN);

	while(1) {
        printf("mini-shell > ");
		fflush(stdout);
        struct cmdline *ligneCommande;
        ligneCommande = readcmd();
        if (ligneCommande->err == NULL) {
        
            //ne rien faire au cas d'un simple retour à la ligne.
            if (ligneCommande->seq[0] == NULL) {
            } 
            //La commande interne "exit"
            else if (!strcmp(ligneCommande->seq[0][0], "exit")) {
                exit(1);
            }
            //La commande interne "cd"
            else if (!strcmp(ligneCommande->seq[0][0], "cd")) {

                change_directory(ligneCommande->seq[0]);
            }
            // La commande "list"
            else if (!strcmp(ligneCommande->seq[0][0], "list")) {
                lister_jobs();
            }
            // La commande stop id .
            else if (!strcmp(ligneCommande->seq[0][0], "stop")) {
                stop_processus(ligneCommande->seq[0]);
            }
        
            // La commande bg id .
            else if (!strcmp(ligneCommande->seq[0][0], "bg")) {
                background(ligneCommande->seq[0]);
            }
            // La commande fg id .
            else if (!strcmp(ligneCommande->seq[0][0], "fg")) {
                foreground(ligneCommande->seq[0]);
            }

            /* Création d'un processus fils.*/
            else if (ligneCommande->seq[1] == NULL){
                pid_t pid_fils = fork();

		        if (pid_fils == -1) {
                    printf("Erreur fork\n");
                    exit(1);
                }
                if (pid_fils == 0){
                    //fils 
                    if (ligneCommande->in != NULL) {
                        /*redirection de l'entrée standard*/
                        rediriger_entree(ligneCommande->in);
                    }
                    if (ligneCommande->out != NULL) {
    
                        /*redirection de la sortie standard*/
                        rediriger_sortie(ligneCommande->out);
                    }
                    
                    int retour = execvp(ligneCommande->seq[0][0], ligneCommande->seq[0]);
                    if (retour == -1) {
                        
                        perror("Erreur ");
                    }
                    exit(1);
                } else {
                    
                    /* BG ou FG ?*/
                    if (ligneCommande->backgrounded == NULL){
                        //FG
                        /* Enregistrer le processus fils dans liste_jobs. */
                        add_job(pid_fils, *ligneCommande->seq[0], FG);
                        pid_fg = pid_fils;
                        wait(&wstatus1);
                        delete_job(pid_fils);
                        
                    } else {
                        /* Enregistrer le processus fils dans liste_jobs. */
                        add_job(pid_fils, *ligneCommande->seq[0], BG);
                        /*afficher l'ID et PID du processus en BG*/
                        printf("[%d]       %d   \n" ,liste_jobs.list[liste_jobs.N - 1].job_id ,liste_jobs.list[liste_jobs.N - 1].job_pid);
                    }
	            }
            }   
            /*le cas des tubes */ 
            else {
                /*calcul nbr tubes */
                int nbr_tubes = 1;
                while( ligneCommande->seq[nbr_tubes] != NULL) {
                    nbr_tubes++;
                }
                nbr_tubes--;
                int wstatus, pid;
                int pipes[2*nbr_tubes];
                for (int i = 0; i< nbr_tubes; i++) {
                    if (pipe(pipes + i*2) < 0) {
                        perror("erreur pipe");
                        exit(1);
                    }
                }
                int j = 0;
                int k = 0;// l'indice de la commande
                while (ligneCommande->seq[k] != NULL) {
                    pid = fork();
                    if (pid < 0) {
                        perror("erreur fork");
                        exit(1);
                    }
                    else if (pid == 0) {
                        //les redirections < et > pour la première et dernière commande
                        if (k == 0) {
                            if (ligneCommande->in != NULL) {
                                /*redirection de l'entrée standard*/
                                rediriger_entree(ligneCommande->in);
                            }
                        }
                        if ( k == nbr_tubes ) {
                            if (ligneCommande->out != NULL) {
    
                                /*redirection de la sortie standard*/
                                rediriger_sortie(ligneCommande->out);
                            }
                        }
                        //si ce n'est pas la peremiere commande , 
                        //rediriger l'entrée standard vers le sortie du dernier tube pipes[j-2]
                        if (j != 0) {
                            if (dup2(pipes[j-2], 0) < 0 ) {
                                perror("erreur dup2");
                                exit(1);
                            }
                        }
                        //si ce n'est pas la derniére commande, 
                        //rediriger la sortie standard vers l'entrée pipes[j+]
                        if (ligneCommande->seq[k+1] != NULL) {
                            if (dup2(pipes[j+1], 1) < 0) {
                                perror("erreur dup2");
                                exit(1);
                            }
                        }
                        //fermer les tubes après les redirections
                        for (int i =0; i < 2*nbr_tubes; i++) {
                            close(pipes[i]);
                        }
                        if (execvp(ligneCommande->seq[k][0], ligneCommande->seq[k]) < 0) {
                            perror("erreur exec");
                            exit(1);
                        }
                    }
                    k++; j+=2;
                }
                //le père ferme tous les tubes et attend ses fils
                for (int i =0; i < 2*nbr_tubes; i++) {
                    close(pipes[i]);
                }
                for (int i =0; i < nbr_tubes +1; i++) {
                    wait(&wstatus);
                }
        }
    }
}
}

