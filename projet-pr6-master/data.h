#ifndef DATA_H_
#define DATA_H_

/**************************************************************************************************
 *                                     CONSTANTES                                                 *
 **************************************************************************************************/

// CONTRAINTES DE TAILLE
#define SIZE_MESS             100     // Taille maximale d'un message entre un client et le serveur.
#define SIZE_PSEUDO           10      // Taille maximale d'un pseudo.
#define SIZE_CODEREQ          5       // Taille d'un code d'une requête en bit.
#define SIZE_ID               11      // Taille d'un ID en bit.
#define SIZE_HEADER           2       // Taille d'un en-tête (CODEREQ + ID) en octet.
#define SIZE_NUMFIL           2       // Taille d'un numéro de fil de discussion en octet.
#define SIZE_NUMBLOC          2       // Taille d'un numéro de bloc en octet.
#define SIZE_NB               2       // Taille du numéro de port ou du nombre de message en octet.
#define SIZE_O_DATALEN        1       // Taille de la DATA en octet.
#define SIZE_B_DATALEN        8       // Taille de la DATA en bit.
#define SIZE_PAQUET           512     // Taille d'un paquet UDP en octet
#define SIZE_FILE             1 << 25 // Taille maximum d'un fichier.
#define SIZE_ADDRMULT         16      // Taille de l'adresse IPv6 de multidiffusion en octet
#define TIMEOUT_SEC           5       // Temps d'attente du serveur en UDP
#define TIMEOUT_USEC          0       // Temps d'attente du serveur en UDP


// CODES DE REQUETES
#define CODEREQ_INSC    1    // Code de la requête pour une inscription.
#define CODEREQ_POST    2    // Code de la requête pour poster un billet.
#define CODEREQ_LIST    3    // Code de la requête pour lister des billets.
#define CODEREQ_ABON    4    // Code de la requête pour s'abonner à un fil.
#define CODEREQ_AJOU    5    // Code de la requête pour ajouter un fichier.
#define CODEREQ_TELE    6    // Code de la requête pour télécharger un fichier.
#define CODEREQ_ERRE    31   // Code de la requête pour les erreurs.
 
// MASQUES
#define MASK_ID               0xFFE0  // Masque binaire pour extraire les 11 derniers bits de l'en-tête.  
#define MASK_CODEREQ          0x1f   // Masque binaire pour extraire les 5 derniers bits de l'en-tête.  

// COMMANDE
#define CMD_INSC        "INSCRIPTION" 
#define CMD_CONN        "CONNEXION"
#define CMD_DECO        "DECONNEXION"
#define CMD_AIDE        "AIDE"
#define CMD_QUIT        "QUITTER"
#define CMD_POST        "POSTER"
#define CMD_LIST        "LISTER"
#define CMD_ABON        "ABONNER"
#define CMD_AJOU        "AJOUTER"
#define CMD_TELE        "TELECHARGER"

/**************************************************************************************************
 *                                           STRUCTURE                                            *
 **************************************************************************************************/

typedef struct user {
    uint16_t id;
    char name[SIZE_PSEUDO];
    struct user *next;
} user;

typedef struct billet {
    uint16_t id_auteur;
    uint16_t datalen;
    char* data;
    struct billet* prec;
} billet;

typedef struct fil {
    uint16_t num_fil;
    uint16_t id_createur;
    uint16_t nb_billets;
    struct billet* last_billet;
    struct fil* next;
    int port_mult;
    char * addr_mult;
    int nb_abonnes;
} fil;

typedef struct Fichier {
    char nom[20];
    size_t taille;
    void* donnees;
    struct Fichier* suivant;
} fichier;

typedef struct Packet {
    int numbloc;
    int taille;
    char *data;
    struct Packet* next;
} Packet;

/**************************************************************************************************
 *                                         FONTIONS                                               *
 **************************************************************************************************/

/*
 * Crée un nouvel utilisateur et l'ajoute dans la liste
 * @param[in]   users   Liste des utilisateurs répertoiriés
 * @param[in]   id      ID du nouvel utilisateur à ajouter
 * @param[in]   name    Pseudo du nouvel utilisateur à ajouter
 */
user* add_user(user* users, int id, char *name);

/*
 * Ajouter un billet dans un fil de discussion.
 * @param[in]   fils     Liste des fils de discussion.
 * @param[in]   data     Contenenu du billet.
 * @param[in]   id       ID de l'auteur.
 * @param[in]   numfil   Numero du fil auquel le billet sera rajoute.
 * @return    Retourne 0 en cas d'erreur,
 *            ou le numero du fil auquel le billet a ete ajoute en cas de succes.
 */
fil * add_billet(fil* fils, char* data, int id, int numfil, int* res);

/*
 * Cherche le nom d'un utilisateur à partir de son ID.
 * @param[in]   users    Liste des utilisateurs.
 * @param[in]   id       ID de l'utilisateurs recherche.
 * @return   Le pseudo de l'utilisateur ou NULL.
 */
char* id_to_username(user* users, uint16_t id);

/*
 * Compte le nombre d'utilisateurs inscrits.
 * @param[in]   users   Tête de la liste des utilisateurs.
 * @return    Retourne le nombre d'utilisateurs inscrits.
 */
int number_user(user* users);

/*
 * Compte le nombre de fils en cours.
 * @param[in]   fils   Tête de la file de fils.
 * @return    Retourne le nombre de fils en cours.
 */
int number_fil(fil* fils);

/*
 * Recuperer un fil particulier.
 * @param[in]    fils      Liste des fils
 * @param[in]    numfil    Le numero de fil qu'on souhaite consulté
 * @return    Le fil n°<numfil> s'il existe, ou NULL sinon.
 */
fil* get_fil(fil* fils, int numfil);

/*
 * Recuperer un fichier particulier.
 * @param[in]    files      Liste des fichiers sur le serveur
 * @param[in]    name      Le numero de fil qu'on souhaite consulté
 * 
 * @return    Le fichier recherché ou NULL s'il existe, ou NULL sinon.
 */
fichier* get_file(fichier* files, char* name);

#endif