#include "client.h"
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>
#include <string.h>

#define MAXMSG MAXREP

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

//////////////////////////////////
// Utils: Fonction de déboggage //
//////////////////////////////////

int deleguer_a_l_invite_interactif(char* reponse, char* message) {
    bool quit = false;
    while (!quit) {
        int ret;
        printf ("> ");
        fflush (stdout);

        if (!fgets (message, MAXMSG, stdin)) {
            break;
        }
        if (message[0] == '\n') {
            continue;
        }
        if (!strcmp(message, "quit")) {
            quit = true;
            puts("bye bye...");
        } else {
            ret = envoyer_recevoir(message, reponse);
            prefix_print(stderr, COL_RECU, reponse);
            if (ret < 0) {break;}
        }
    }

    exit(0);
}

////////////////////////////////////
// Utils: manipulation de chaînes //
////////////////////////////////////

/**
 * Cette structure englobe une chaîne et sa longueur, pour limiter les strlen
 * et simplifier les mutations (ajout/recupération de caractères par la gauche
 * ou la droite).
 * 
 * Certaines des méthodes partent du principe que de l'espace libre et en notre
 * possession est disponible à gauche ou à droite; aucun déplacement /
 *  réalocation n'est effectué.
 * Par exemple, pour `push_end`, il est trivial qu'un octet doit être disponible
 * à droite du dernier caractère.
 * 
 * len: la longueur de la chaîne
 * inner: la chaîne. Elle n'a techniquement pas besoin de finir par 0, mais
 *   ça reste idéal pour l'interopérabilité des types.
 */
typedef struct {
    int len;
    char* inner;
} str;

/**
 * Constructeur naïf
 */
str new_str(char* inner) {
    str s = { .len = strlen(inner), .inner = inner };
    return s;
}

// La fonction part du principe que la chaîne est initialisée à 0 à droite
void push_end(str* s, char c) {
    s->inner[s->len] = c;
    s->len++;
}

// La fonction part du principe que la chaîne dispose de place avant son premier
// élément
void push_start(str* s, char c) {
    s->inner = &s->inner[-1];
    s->inner[0] = c;
    s->len++;
}

char pull_end(str* s) {
    if (s->len == 0) {
      printf("s->len == 0");
      exit(1);
    }
    char c = s->inner[s->len - 1];
    s->inner[s->len - 1] = 0;
    s->len--;
    return c;
}

char pull_start(str* s) {
    char c = s->inner[0];
    s->inner = &s->inner[1];
    s->len--;
    return c;
}

//////////////////
// Utils: César //
//////////////////

void crypteCesar(char* txt, int decalage) {
    for (int i = 0; txt[i] != 0; i++) {

        char offset = 0;
        
        if ('a' <= txt[i] && txt[i] <= 'z') {
            offset = 'a';
        }

        if ('A' <= txt[i] && txt[i] <= 'Z') {
            offset = 'A';
        }

        if (offset > 0) {
            txt[i] = (txt[i] - offset + 26 + decalage) % 26 + offset;
        }
    }
}

void decrypteCesar(char* txt, int decalage) {
    crypteCesar(txt, -decalage);
}

///////////////////////
// Utils: crypteMove //
///////////////////////

void crypteMove(char* txt) {
    // On garde une réf. vers la destination de txt, qu'on ne modifiera pas
    char* txt_ = txt;
    char enc[5000];

    int txtlen = strlen(txt);

    while (txtlen != 0) {
        char c = txt[0];
        // On décale le pointeur pour les itérations suivantes
        txt += 1;

        enc[strlen(enc)] = c;
        txtlen -= 1;
        char x = c % 8;
        if (txtlen > x) {
            for (int i = 0; i < x; i++) {
                char tmp = txt[0];
                // c.f. commentaire ci-dessus
                txt += 1;
                
                txt[strlen(txt)] = tmp;
            }
        }
    }

    strcpy(txt_, enc);
}

/**
 * Cette version de la fonction prend la taille à allouer en paramètre, pour
 * permettre à OneMillion d'utiliser beaucoup plus de RAM (pour réduire la
 * complexité et le nombre d'allocations en contrepartie)
 * Une version fixée à 10Ko se trouve en dessous.
 */
void decrypteMoveS(char *source, long bufsize) {
    char* buffer = malloc((long)bufsize);
    // On prend comme position 0 la moitié du buffer, ça permettra de décaler
    // ce même poiteur vers la gauche au fur et à mesure qu'on rajoute des
    // caractères au début.
    char *enc_ = &buffer[(long)bufsize / 2];
    
    strcpy(enc_, source);
    
    str enc = new_str(enc_);

    char *dec_ = malloc((long)bufsize);
    
    str dec = { .len = 0, .inner = &dec_[(long)bufsize / 2] };
    
    while (enc.len != 0) {
        char c = pull_end(&enc);
        int x = c % 8;

        if (dec.len > x) {
            for (int i = 0; i < x; i++) {
                char dernier = pull_end(&dec);
                push_start(&dec, dernier);
            }
        }
        
        push_start(&dec, c);
    }
    
    strcpy(source, dec.inner);
}

/**
 * Version pratique de decrypteMove, fixée à une valeur généralement
 * suffisante. On pourrait également la réimplémenter pour qu'elle alloue sur
 * la pile, mais c'est pas pratique à faire en C (à l'inverse du Rust).
 */
void decrypteMove(char *source) {
    decrypteMoveS(source, 10000);
}

//////////////////////
// Utils: crypteSeq //
//////////////////////

#define NOT_FOUND -1

int find(char* txt, char c) {
    for (int i = 0; txt[i] != 0; i++) {
        if (c == txt[i]) return i;
    }
    
    return NOT_FOUND;
}

void crypteSeq(char* message) {
    int alphabet_len = 0;
    char alphabet[100] = "";
    
    for (int i = 0; message[i] != 0; i++) {
        char current = message[i];
        int current_pos = find(alphabet, current);
        
        if (current_pos == NOT_FOUND) {
            current_pos = alphabet_len;
            alphabet[current_pos] = current;
            alphabet_len++;
        } else {
            char d;
            if (alphabet[0] == current) {
                d = alphabet[alphabet_len - 1];
            } else {
                d = alphabet[current_pos - 1];
            }
            message[i] = d;
        }
        
        // On décale le tableau pour n > current_pos vers le gauche
        for (int j = current_pos; j < (alphabet_len - 1); j++) {
            alphabet[j] = alphabet[1+j];
        }
        alphabet[alphabet_len - 1] = current;
    }
}

void decrypteSeq(char *message) {
  int alphabet_len = 0;
  char alphabet[200] = "";

  for (int i = 0; message[i] != 0; i++) {
    char current = message[i];
    int current_pos = find(alphabet, current);

    if (current_pos == NOT_FOUND) {
      current_pos = alphabet_len;
      alphabet[current_pos] = current;
      alphabet_len++;
    } else {
      char d;

      if (alphabet[alphabet_len - 1] == current) {
        current_pos = 0;
        d = alphabet[0];
      } else {
        current_pos++;
        d = alphabet[current_pos];
      }

      message[i] = d;
      current = d;

      // On décale le tableau pour n > current_pos vers le gauche
    for (int j = current_pos; j < (alphabet_len - 1); j++) {
      alphabet[j] = alphabet[1 + j];
    }
    alphabet[alphabet_len - 1] = current;
    }
  }
}

//////////////
// tutoriel //
//////////////

void main_t1(char* reponse, char* message) {
    envoyer_recevoir("load tutoriel", reponse);
    envoyer_recevoir("aide", reponse);
    envoyer_recevoir("start", reponse);
    envoyer_recevoir("oui", reponse);
    envoyer_recevoir("4", reponse);
    envoyer_recevoir("blanc", reponse);
    envoyer_recevoir("Pincemoi", reponse);
    envoyer_recevoir("tutoriel", reponse);
}

///////////////
// tutoriel2 //
///////////////

void main_t2(char* reponse, char* message) {
    envoyer_recevoir("load tutoriel2", reponse);
    envoyer_recevoir("start", reponse);

	for (char i = 0; i < 5; i++) {
		sprintf(message, "%d", 2 * atoi(reponse));
		envoyer_recevoir(message, reponse);
	}
}

///////////////
// tutoriel3 //
///////////////

void MAJUSCULE(char* c) {
    if (*c >= 'a' && *c <= 'z') {
        *c = *c - 'a' + 'A';
    }
}

void main_t3(char* reponse, char* message) {
    envoyer_recevoir("load tutoriel3", reponse);
    envoyer_recevoir("depart", reponse);
    envoyer_recevoir("OK", reponse);
    envoyer_recevoir("OUI", reponse);

    for (char i = 0; i < 6; i++) {
        for (int j = 0; reponse[j] != 0; j++) {
            MAJUSCULE(&reponse[j]);
        }
        envoyer_recevoir(reponse, reponse);
    }
}

/////////////
// projetX //
/////////////

void main_projetx(char* reponse, char* message) {
    envoyer_recevoir("load projetX", reponse);
    envoyer_recevoir("start", reponse);
    envoyer_recevoir("veni vidi vici", reponse);
}

///////////
// planB //
///////////

void main_planb(char* reponse, char* message) {
    envoyer_recevoir("load planB", reponse);
    envoyer_recevoir("start", reponse);
    envoyer_recevoir("aide", reponse);

    // En clair, le 1er caractère devrait valoir 'C'
    char premier_car = reponse[0];
    int decalage = 'C' - premier_car;

    char pw[] = "hasta la revolucion";
    decrypteCesar(pw, -decalage);
    envoyer_recevoir(pw, reponse);

    // 5402 2586 9910 4327
}

////////////////
// crypteMove //
////////////////

void main_cm(char* reponse, char* message) {
    envoyer_recevoir("load crypteMove", reponse);

    char reponse2[MAXREP] = "";
    envoyer_recevoir("help", reponse2);
    envoyer_recevoir("start", reponse);
    crypteMove(reponse2);

    envoyer_recevoir(reponse2, reponse);
    printf("%s\n", reponse);
}

///////////////
// BayOfPigs //
///////////////

void main_bop(char* reponse, char* message) {
    envoyer_recevoir("load BayOfPigs", reponse);
    envoyer_recevoir("start", reponse);
    decrypteMove(reponse);
    printf("%s\n", reponse);

    envoyer_recevoir("Par otuam eriet", reponse);
    decrypteMove(reponse);
    printf("%s\n", reponse);
}

////////////////
// OneMillion //
////////////////

void main_om(char* reponse, char* message) {
    envoyer_recevoir("load OneMillion", reponse);
    decrypteMove(reponse);
    printf("%s\n", reponse);

    char aide[101];
    envoyer_recevoir("aide", aide);

    char cle[101];
    envoyer_recevoir("start", cle);

    char chaine[1000001] = "";

    for (int i = 0; i < 9999 * 100; i++) {
        chaine[i] = aide[i % 100];
    }

    strcpy(&chaine[9999 * 100], cle);
    decrypteMoveS(chaine, 50000000);

    chaine[100] = 0;
    envoyer_recevoir(chaine, reponse);
    decrypteMove(reponse);

    printf("%s\n", reponse);

    // ZXBV-UTMT-ACOX-KKEW-SMLT
}

///////////////
// crypteSeq //
///////////////

void main_cs(char* reponse, char* message) {
    envoyer_recevoir("load crypteSeq", reponse);
    decrypteMove(reponse);
    printf("%s\n", reponse);

    envoyer_recevoir("start", reponse);
    decrypteMove(reponse);
    crypteSeq(reponse);

    envoyer_recevoir(reponse, reponse);

    // La réponse commence par une partie en clair. On récupère la réponse à
    // partir du premier caractère "crypté"
    int first_A = find(reponse, '\n') + 2;
    char* rep = &reponse[first_A];
    decrypteSeq(rep);
    printf("%s\n", rep);
}

////////////////
// Northwoods //
////////////////

/**
 * Renvoie le mot de passe de l'exo Northwoods. La chaîne en paramètre est
 * "consommée", c-a-d que cette fonction la modifie pour des raisons de
 * simplicité et limiter l'utilisation de RAM.
 * La fonction renvoie toujours une valeur correcte (pointeur non-nul & chaîne
 * de 20 caractères) et fera planter le programme en cas d'entrée invalide.
 */
char* nw_find_password(char* mut_reponse) {
    static int PASSWORD_LEN = 20;

    for (int i = 2; mut_reponse[i + PASSWORD_LEN + 1] != 0; i++) {
        if (
            mut_reponse[i-2] == ' '
            && mut_reponse[i-1] == '\''
            && mut_reponse[i+PASSWORD_LEN] == '\''
        ) {
            // On définit la fin de la chaîne
            mut_reponse[i+PASSWORD_LEN] = 0;

            return &mut_reponse[i];
        }
    }

    printf("Aucun mot de passe trouvé");
    exit(1);
}

void main_nw(char* reponse, char* message) {
    envoyer_recevoir("load Northwoods", reponse);
    envoyer_recevoir("start", reponse);

    envoyer_recevoir("hasta la victoria siempre", reponse);
    decrypteSeq(reponse);
    printf("\n%s\n", reponse);

    char* pw = nw_find_password(reponse);

    envoyer_recevoir(pw, reponse);
    decrypteSeq(reponse);
    printf("\n%s\n", reponse);

    char pw2[] = "There will be no Nineteen Eighty-Four";
    crypteSeq(pw2);
    envoyer_recevoir(pw2, reponse);
    int start = find(reponse, 'B');
    decrypteSeq(&reponse[start]);
    printf("\n%s\n", reponse);
}

/////////////////////
// Fonction `main` //
/////////////////////

// Pointeur vers la fonction de lancement d'un exercice
typedef void (*exo_main_func) (char* reponse, char* message);

// Un "exo" est un couple de son nom et du pointeur vers sa fonction de lancement
typedef struct {
    char* nom;
    exo_main_func main;
} exo;

// Liste statique des exos
#define EXERCICES_LEN (sizeof(EXERCICES) / sizeof(exo))
static exo EXERCICES[] = {
    { "tutoriel", main_t1 },
    { "tutoriel2", main_t2 },
    { "tutoriel3", main_t3 },

    { "projetX", main_projetx },
    { "planB", main_planb },
    { "crypteMove", main_cm },
    { "BayOfPigs", main_bop },
    { "OneMillion", main_om },
    { "crypteSeq", main_cs },
    { "Northwoods", main_nw },
};

// Point d'entrée
int main(int argc, char** argv) {
    printf("INF301 -- APP1 -- DECORSAIRE Mattéo & ONGHENA Edgar\n\n");

    if (argc != 2) {
        printf(" \e[31mUtilisation: app1 <nom d'exercice>\n Exercices disponibles:\n");
        for (int i = 0; i < EXERCICES_LEN; i++) {
            printf("  * %s\n", EXERCICES[i].nom);
        }
        return 1;
    }

    exo_main_func exo_main = NULL;
    for (int i = 0; i < EXERCICES_LEN; i++) {
        if (0 == strcmp(EXERCICES[i].nom, argv[1])) {
            exo_main = EXERCICES[i].main;
            break;
        }
    }

    if (exo_main == NULL) {
        printf(" \e[31mNom d'exercice inconnu: %s\n", argv[1]);
        return 2;
    }

    // Préparation de la connection

    char reponse[MAXREP];
    char message[MAXMSG];

    mode_debug(true);
    connexion("im2ag-appolab.u-ga.fr", 9999);
    envoyer_recevoir("login 11902798 ONGHENA", reponse);

    // Appel de la fonction de résolution de l'exo demandé
    exo_main(reponse, message);

    // Fin

    envoyer_recevoir(" ", reponse); // Normalement, appolab devrait se deconnecter là
}
