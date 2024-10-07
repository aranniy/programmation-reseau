#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <net/if.h>

#include "data.h"

/*
 * Se connecter à un serveur à partir d'une adresses de noms et d'un port.
 * 
 * @param[in]   hostname   Le nom du serveur.
 * @param[in]   port       Le port d'écoute du serveur.
 * @param[in]   sock       La socket de connexion au serveur
 * @param[in]   addrlen    La taille de la socket.
 * 
 * @return  Retourne 0 si la connexion au serveur est réussi, 
 *          -1 si l'adresse n'existe pas,
 *          ou -2 s'il y a eu une erreur lors de la création de la socket.
 */
int connexion(char* hostname, char* port, int* sock, int* addrlen) {

  struct addrinfo hints, *r, *p;
  int ret;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET6;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_V4MAPPED | AI_ALL;

  if ((ret = getaddrinfo(hostname, port, &hints, &r)) != 0 || NULL == r){
    fprintf(stderr, "Erreur getaddrinfo : %s\n", gai_strerror(ret));
    return -1;
  }
  
  *addrlen = sizeof(struct sockaddr_in6);
  p = r;
  while( p != NULL ) {
    if((*sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) > 0){
      if(connect(*sock, p->ai_addr, *addrlen) == 0) 
        break;
      close(*sock);
    }
    p = p -> ai_next;
  }

  if (NULL == p) return -2;

  // On libère la mémoire allouée par getaddrinfo 
  if (r)
    freeaddrinfo(r);

  return 0;
}

/*
 * S'incrire à 'Mégaphone'
 * @param[in]   hostname     Le nom du serveur.
 * @param[in]   port         Le port d'écoute du serveur.
 * @param[in]   id           L'ID de l'utilisateur.
 * @param[in]   pseudo       La pseudo de l'utilisateur.
 * @param[in]   pseudo_len   La taille du pseudo de l'utilisateur.
 */
void inscription(char* hostname, char* port, uint16_t* id, char* pseudo, int* pseudo_len) {

  /*** CONNEXION AU SERVEUR ***/

  int fdsock, adrlen;
    
  switch (connexion(hostname, port, &fdsock, &adrlen)) {
    case 0: break;
    case -1:
      fprintf(stderr, "Erreur: hote non trouve.\n");  
      exit(1);
    case -2:
      fprintf(stderr, "Erreur: echec de creation de la socket.\n");
      exit(1);
  }
    
  /*** INSCRIPTION ***/
  
  printf("Entrez votre nom d'utilisateur : ");
  fflush(stdin);
  while(scanf("%10s", pseudo) == EOF) {
    printf("Entrez votre nom d'utilisateur : ");
    scanf("%10s", pseudo);
  }

  printf("Pseudo : %s\n", pseudo);

  *pseudo_len = strlen(pseudo);

  uint16_t header = htons(CODEREQ_INSC);

  char message[SIZE_PSEUDO + 2];
  memcpy(message, &header, SIZE_HEADER);
  
  if (*pseudo_len < 10) {
    memcpy(message + 2, pseudo, strlen(pseudo));
    memset(message + 2 + strlen(pseudo), '#', SIZE_PSEUDO - *pseudo_len);
  } else {
    memcpy(message + 2, pseudo, SIZE_PSEUDO);
  }

  // ENVOIE DU MESSAGE
  int ecrit = send(fdsock, message, sizeof(message), 0);
  if (ecrit <= 0) {
    perror("Erreur d'envoie.");
    exit(4);
  }

  // RECEPTION DU MESSAGE
  char buf[SIZE_MESS + 1];
  memset(buf, 0, SIZE_MESS + 1);
  int recu = recv(fdsock, buf, SIZE_MESS, 0);
  if (recu <= 0){
    perror("Erreur de reception");
    exit(4);
  }

  uint16_t h;
  memcpy(&h, buf, sizeof(uint16_t));

  *id = ntohs(h) >> SIZE_CODEREQ;

  uint16_t code = ntohs(h) & MASK_CODEREQ;

  if(code == CODEREQ_ERRE){
    printf("Erreur d'inscription\n");
  } else {
    printf("Voici votre identifiant : %d\n", *id);
    printf("Retenez-le pour vous reconnecter plus tard !\n");
    printf("Vous êtes maintenant connecté(e) !\n");
  }

  close(fdsock);

}

/*
 * 
 */
int connexion_bis(){
  int id = 0;
  printf("Veuillez entrer votre identifiant : \n");
  scanf("%d", &id);
  return id;
}

/*
 * Poster un billet
 * @param[in]    hostname    Adresse de noms du serveur
 * @param[in]    port        Port d'écoute du service
 * @param[in]     id          ID de l'utilisateur
 */
void poster(char* hostname, char* port, uint16_t* id) {

  uint16_t header = htons((*id << SIZE_CODEREQ) | CODEREQ_POST);

  int n = 0;
  printf("Sur quel fil de discussion voulez-vous poster votre billet ? \nTapez 0 si vous voulez créer un nouveau fil.\n");

  while (scanf("%d", &n) != 1) {
    printf("Entrée invalide. Veuillez entrer un nombre.\n");
    while (getchar() != '\n') {
        continue;
    }
  }

  uint16_t numfil = htons(n);

  char data[SIZE_MESS] = "";

  printf("Veuillez entrer le contenu de votre billet :\n");
  while (strlen(data) == 0) {
        fgets(data, sizeof(data), stdin);
        // Supprimer le caractère de fin de ligne ('\n') de la saisie
        if (data[strlen(data) - 1] == '\n') {
            data[strlen(data) - 1] = '\0';
        }
    }

  int datalen = strlen(data);

  char message[SIZE_MESS];
  memset(message, 0, SIZE_MESS);
  memcpy(message, &header, SIZE_HEADER);
  memcpy(message + SIZE_HEADER, &numfil, SIZE_NUMFIL);
  memcpy(message + SIZE_HEADER + SIZE_NUMFIL + SIZE_NB, &datalen, SIZE_O_DATALEN);
  memcpy(message + SIZE_HEADER + SIZE_NUMFIL + SIZE_NB + SIZE_O_DATALEN, data, datalen);

  int fdsock, adrlen;

  // CONNEXION AU SERVEUR
  switch (connexion(hostname, port, &fdsock, &adrlen)) {
    case 0: break;
    case -1:
      fprintf(stderr, "Erreur: hote non trouve.\n");  
      exit(1);
    case -2:
      fprintf(stderr, "Erreur: echec de creation de la socket.\n");
      exit(1);
  }

  // ENVOI DU MESSAGE
  if (send(fdsock, message, sizeof(message), 0) <= 0) {
    perror("Erreur d'envoie ");
    exit(4);
  }

  // RECEPTION DU MESSAGE
  char buf[SIZE_MESS];
  memset(buf, 0, SIZE_MESS);
  int recu = recv(fdsock, buf, SIZE_MESS, 0);
  if (recu <= 0){
    perror("Erreur de reception");
    exit(4);
  }

  uint16_t h;
  memcpy(&h, buf, sizeof(h));

  uint16_t new_numfil;
  memcpy(&new_numfil, buf + SIZE_HEADER, sizeof(uint16_t));

  new_numfil = ntohs(new_numfil);

  if(header == h && (n == 0 || n == new_numfil)){
    printf("Votre billet a bien été posté sur le fil n°%d !\n", new_numfil);
  } else {
    printf("Erreur postage de billet\n");
  }
  close(fdsock);
}

/*
 * Lister les billets
 * @param[in]  hostname   L'adresse de noms du serveur.
 * @param[in]  port       Le port d'écoute du serveur.
 * @param[in]  id         L'ID de l'utilisateur.
 */
void lister(char* hostname, char* port, uint16_t* id) {

  // L'utilisateur n'est pas connecté
  if(*id == 0) {
    printf("Vous n'êtes pas connecté(e) !\n");
  
  // L'utilisateur est connecté
  } else {

    uint16_t header = htons((*id << SIZE_CODEREQ) | CODEREQ_LIST);
    uint16_t numfil = 0,
             nbr_billets = 0,
             datalen = 0;

    char reponse[SIZE_MESS];

    printf("Quel fil de discussion souhaitez-vous consulter ?\n");

    memset(reponse, 0, SIZE_MESS);
    fflush(stdin);

    while(scanf("%hd", &numfil) == EOF || numfil < 0) {
      printf("Vous devez entrer le numéro du fil de discussion que vous souhaitez consulter.\n");
      scanf("%hd", &numfil);
    }
    numfil = htons(numfil);

    printf("Combien de billets voulez-vous afficher ?\n");

    memset(reponse, 0, SIZE_MESS);
    fflush(stdin);

    while(scanf("%hd", &nbr_billets) == EOF || nbr_billets < 0) {
      printf("Vous devez entrer le nombre de billets que vous souhaitez consulter.\n");
      scanf("%hd", &nbr_billets);
    }
    nbr_billets = htons(nbr_billets);

    // CREATION DU MESSAGE
    char message[SIZE_MESS];
    memset(message, 0, SIZE_MESS);

    memcpy(message, &header, SIZE_HEADER);
    memcpy(message + SIZE_HEADER, &numfil, SIZE_NUMFIL);
    memcpy(message + SIZE_HEADER + SIZE_NUMFIL, &nbr_billets, SIZE_NB);
    memcpy(message + SIZE_HEADER + SIZE_NUMFIL + SIZE_NB, &datalen, SIZE_O_DATALEN);

    /*** CONNEXION AU SERVEUR ***/
  
    int fdsock, adrlen;
      
    switch (connexion(hostname, port, &fdsock, &adrlen)) {
      case 0: break;
      case -1:
        fprintf(stderr, "Erreur: hote non trouve.\n");  
        exit(1);
      case -2:
        fprintf(stderr, "Erreur: echec de creation de la socket.\n");
        exit(1);
    }

    // ENVOIE DU MESSAGE
    int ecrit = send(fdsock, message, sizeof(message), 0);
    if (ecrit <= 0) {
      perror("Erreur d'envoi");
      exit(4);
    }

    // RECEPTION DU PREMIER MESSAGE DU SERVEUR
    char buf[SIZE_MESS];
    memset(buf, 0, SIZE_MESS);

    int recu = recv(fdsock, buf, SIZE_MESS, 0);
    if (recu <= 0){
      perror("Erreur de reception");
      exit(4);
    }

    memcpy(&header, buf, SIZE_HEADER);
    header = ntohs(header);

    uint16_t codereq =  header & MASK_CODEREQ;

    if(codereq == 31) {
      printf("Le serveur à rencontrer un problème.");

    } else {

      uint16_t nb;
      memcpy(&nb, buf + SIZE_HEADER + SIZE_NUMFIL, SIZE_NB);
      nb = ntohs(nb);

      // RECEPTION DES AUTRES MESSAGES
      for(int i = 0; i < nb; i++) {

        int debut_message = SIZE_NUMFIL + (2 * SIZE_PSEUDO) + SIZE_O_DATALEN;
        char buf2[debut_message];
        memset(buf2, 0, debut_message);

        int recu = recv(fdsock, buf2, debut_message, 0);
        if (recu < 0){
          perror("Erreur de reception");
          exit(4);
        }

        // NUMERO DU FIL
        memset(&numfil, 0, SIZE_NUMFIL);
        memcpy(&numfil, buf2, SIZE_NUMFIL);
        numfil = ntohs(numfil);
        // CREATEUR DU FIL
        char origine[SIZE_PSEUDO + 1];
        memset(origine, 0, sizeof(origine));
        memcpy(origine, buf2 + SIZE_NUMFIL, SIZE_PSEUDO);
        printf("- FIL N°%d INITIÉ PAR %s -\n", numfil, origine);

        // AUTEUR DU BILLET
        char pseudo[SIZE_PSEUDO + 1];
        memset(pseudo, 0, sizeof(pseudo));
        memcpy(pseudo, buf2 + SIZE_NUMFIL + SIZE_PSEUDO, SIZE_PSEUDO);

        // LONGUEUR DU BILLET
        uint8_t datalen;
        memset(&datalen, 0, SIZE_O_DATALEN);
        memcpy(&datalen, buf2 + SIZE_NUMFIL + (2 * SIZE_PSEUDO), SIZE_O_DATALEN);

        char data[datalen];

        int recu2 = recv(fdsock, data, datalen, 0);
        if (recu2 < 0){
          perror("Erreur de reception");
          exit(4);
        }

        printf("Billet n°%d\n", i + 1);
        printf("> Posté par %s\n", pseudo);
        printf("%s\n", data);
        printf("--------------------------\n");

        memset(data, 0, datalen);

      }

    }
    
    close(fdsock);
  }
}

/*
 * S'abonner à un fil
 * @param[in]    hostname    Adresse de noms du serveur
 * @param[in]    port        Port d'écoute du service
 * @param[in]    interface   Nom de l'interface de multidiffusion
 * @param[in]    id          ID de l'utilisateur
 */
void abonner(char* hostname, char* port, char* interface, uint16_t* id ) {

  int n = 0;
  printf("A quel fil de discussion voulez-vous vous abonner ?\n");

  while (scanf("%d", &n) != 1 || n<=0) {
    printf("Entrée invalide. Veuillez entrer un nombre.\n");
    while (getchar() != '\n') {
        continue;
    }
  }

  uint16_t header = htons((*id << SIZE_CODEREQ) | CODEREQ_ABON);
  uint16_t numfil = htons(n);

  int taille_message = SIZE_HEADER + SIZE_NUMFIL + SIZE_NB + SIZE_O_DATALEN;
  char message[taille_message];

  memset(message, 0, taille_message);
  memcpy(message, &header, SIZE_HEADER);
  memcpy(message + SIZE_HEADER, &numfil, SIZE_NUMFIL);

  int fdsock, adrlen;

  // CONNEXION AU SERVEUR
  switch (connexion(hostname, port, &fdsock, &adrlen)) {
    case 0: break;
    case -1:
      fprintf(stderr, "Erreur: hote non trouve.\n");  
      exit(1);
    case -2:
      fprintf(stderr, "Erreur: echec de creation de la socket.\n");
      exit(1);
  }

  // ENVOI DU MESSAGE
  if (send(fdsock, message, sizeof(message), 0) <= 0) {
    perror("Erreur d'envoie ");
    exit(4);
  }

  // RECEPTION DU MESSAGE
  char buf[SIZE_MESS];
  memset(buf, 0, SIZE_MESS);
  int recu = recv(fdsock, buf, SIZE_MESS, 0);
  if (recu <= 0){
    perror("Erreur de reception");
    exit(4);
  }

  uint16_t h;
  memcpy(&h, buf, SIZE_HEADER);

  uint16_t code = ntohs(h) & MASK_CODEREQ;
  if(code == 31){
    printf("Erreur\n");
    close(fdsock);
    return;
  }

  uint16_t numfil_retour;
  memcpy(&numfil_retour, buf + SIZE_HEADER, SIZE_NUMFIL);

  numfil_retour = ntohs(numfil_retour);

  uint16_t port_retour;
  memcpy(&port_retour, buf + SIZE_HEADER + SIZE_NUMFIL, SIZE_NB);

  char adresse[SIZE_ADDRMULT];
  memcpy(&adresse, buf + SIZE_HEADER + SIZE_NUMFIL + SIZE_NB, SIZE_ADDRMULT);

  int sock = socket(AF_INET6, SOCK_DGRAM, 0);
  if (sock < 0 || (sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    printf("Erreur socket");
    return;
  }

  struct sockaddr_in grsock;
  memset(&grsock, 0, sizeof(grsock));
  grsock.sin_family = AF_INET;
  grsock.sin_addr.s_addr = htonl(INADDR_ANY);
  grsock.sin_port = port_retour;
  if(bind(fdsock, (struct sockaddr*) &grsock, sizeof(grsock))) {
    close(sock);
    return;
  }

  struct ip_mreqn group;
  memset(&group, 0, sizeof(group));
  group.imr_multiaddr.s_addr = inet_addr(adresse);
  group.imr_ifindex = if_nametoindex (interface);
  if(setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &group, sizeof(group)) < 0) {
    perror("echec de l'abonnement");
    close(sock);
    return;
  }

  printf("Vous êtes maintenant abonné au fil n°%d", numfil_retour);

  close(fdsock);
}

void ajouter(char* hostname, char* port, uint16_t* id) {

  /*** CONNEXION AU SERVEUR ***/

  int fdsock, adrlen;
    
  switch (connexion(hostname, port, &fdsock, &adrlen)) {
    case 0: break;
    case -1:
      fprintf(stderr, "Erreur: hote non trouve.\n");  
      exit(1);
    case -2:
      fprintf(stderr, "Erreur: echec de creation de la socket.\n");
      exit(1);
  }

   /*** RECUPERATION DES INFORMATIONS NECESSAIRES ***/

  uint16_t header = htons((*id << SIZE_CODEREQ) | CODEREQ_AJOU);
  uint16_t numfil = -1;
  uint16_t nb = htons(0);
  uint16_t datalen;
  char data[SIZE_MESS - 7];
  char reponse[SIZE_MESS];
  char message[SIZE_MESS];

  printf("Sur quel fil de discussion voulez vous ajouter le fichier ?\n");

  memset(reponse, 0, SIZE_MESS);
  fflush(stdin);
  while(scanf("%s", reponse) == EOF || numfil < 0) { 
    printf("Vous devez entrer le numéro du fil de discussion que vous souhaitez mettre à jour.\n");
    scanf("%s", reponse);
    numfil = atoi(reponse);
  }

  numfil = htons(numfil);

  printf("Veuillez entrer le nom de votre fichier: \n");
  memset(data, 0, SIZE_MESS - 7);
  fflush(stdin);
  while(scanf("%92s", data) == EOF) { 
    printf("Vous devez entrer le nom du fichier que vous voulez ajouter.\n");
    scanf("%92s", data);
  }

  if (access(data, F_OK) == -1) {
    printf("Le fichier n'existe pas.\n");
    exit(EXIT_FAILURE);
  } else {
    printf("Le fichier existe, yay!\n");
  }

  datalen = strlen(data);

  /*** FORMATION DU MESSAGE ***/

  memset(message, 0, SIZE_MESS);
  memcpy(message, &header,SIZE_HEADER);
  memcpy(message + SIZE_HEADER,&numfil,SIZE_NUMFIL);
  memcpy(message + SIZE_HEADER + SIZE_NUMFIL,&nb,SIZE_NB);
  memcpy(message + SIZE_HEADER + SIZE_NUMFIL + SIZE_NB, &datalen, SIZE_O_DATALEN);
  memcpy(message + SIZE_HEADER + SIZE_NUMFIL + SIZE_NB + SIZE_O_DATALEN, &data,datalen);

  /*** ENVOIE DU MESSAGE ***/

  int ecrit = send(fdsock, message, sizeof(message), 0);
  if (ecrit <= 0) {
    perror("Erreur d'envoi");
    exit(4);
  }

  /*** RECEPTION DU MESSAGE ***/

  char buf[SIZE_MESS];
  memset(buf, 0, SIZE_MESS);
  int recu = recv(fdsock, buf, SIZE_MESS, 0);
  if (recu <= 0){
    perror("Erreur de reception");
    exit(4);
  }

  uint16_t h;
  memcpy(&h, buf, sizeof(uint16_t));
  uint16_t code = ntohs(h) & MASK_CODEREQ;
  if(code == CODEREQ_ERRE){
    printf("Erreur lors de la demande d'ajout de fichier.\n");
  } else {
    memcpy(&nb, buf + SIZE_HEADER + SIZE_NUMFIL,SIZE_NB);
    nb = ntohs(nb);
    printf("Voici le port auquel vous devrez tranférer le fichier : %d !\n", nb);
    printf("(pas besoin de le faire manuellement, le service est automatique)\n");
  }

  /*** CONNEXION UDP AU SERVEUR AVEC UN AUTRE PORT ***/

  int sock = socket(PF_INET6, SOCK_DGRAM, 0);
  if (sock < 0) {
    perror("socket() failed");
    exit(-1);
  }

  struct sockaddr_in6 serv_addr;
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin6_family = AF_INET6;
  inet_pton(AF_INET6,"::1", &serv_addr.sin6_addr);
  serv_addr.sin6_port = htons(nb);
  socklen_t serv_addr_len = sizeof(serv_addr);

   /*** ENVOIE DES PAQUETS ***/

  int file_fd = open(data, O_RDONLY);
  if (file_fd < 0) {
    perror("Erreur lors de l'ouverture du fichier");
    exit(-1);
  }

  // Récupération de la taille du fichier
  off_t taille_fichier = lseek(file_fd, 0, SEEK_END);
  lseek(file_fd, 0, SEEK_SET);

  // Vérification de la taille du fichier
  if (taille_fichier > (1 << 25)) {
      printf("Le fichier fait plus de 32 Mo.\n");
      exit(-1);
  }

  // Calcul du nombre de paquets nécessaires
  int nb_paquets = taille_fichier / SIZE_PAQUET;
  if (taille_fichier % SIZE_PAQUET != 0) {
    nb_paquets++;
  }

  // Envoi des paquets en UDP
  int num_bloc = 1;
  ssize_t read_size = 0;

  char buffer[SIZE_PAQUET];
  memset(buffer, 0, sizeof(buffer));

  for (int i = 1; i <= nb_paquets; i++) {

    // Lecture du prochain paquet de données du fichier
    read_size = read(file_fd, buffer,SIZE_PAQUET);
    if (read_size < 0) {
      perror("Erreur lors de la lecture du fichier");
      close(file_fd);
      exit(1);
    }

    // Envoi du paquet en UDP
    char paquet[SIZE_PAQUET + 4];
    memset(paquet,0, sizeof(paquet));
    memcpy(paquet, &header, SIZE_HEADER);
    uint16_t num = htons(num_bloc);
    memcpy(paquet + SIZE_HEADER,&num,SIZE_NUMBLOC);
    memcpy(paquet + SIZE_HEADER + SIZE_NUMBLOC, &data,SIZE_PAQUET);

    int send = sendto(sock, paquet ,SIZE_PAQUET + 4,0,(struct sockaddr *)&serv_addr,serv_addr_len);
    if (send < 0) {
      perror("Erreur lors de l'envoi du paquet");
      close(file_fd);
      exit(1);
    }

    num_bloc++;

  }

  close(file_fd);
  close(sock);
  close(fdsock);
}

/*
 * Télécharger un fichier.
 *
 * @param[in]  hostname   L'adresse de noms du serveur.
 * @param[in]  port       Le port d'écoute du serveur.
 * @param[in]  id         L'ID de l'utilisateur.
 */
void telecharger(char* hostname, char* port, uint16_t* id) {

  // L'utilisateur n'est pas connecté
  if(*id == 0) {
    printf("Vous n'êtes pas connecté(e) !\n");
  
  // L'utilisateur est connecté
  } else {

    uint16_t header = htons((*id << SIZE_CODEREQ) | CODEREQ_TELE);
    uint16_t numfil = 0,
             nb = 0,
             datalen;

    char data[SIZE_MESS - SIZE_HEADER - SIZE_NUMFIL - SIZE_NB - SIZE_O_DATALEN];

    memset(data, 0, sizeof(data));

    printf("Quel fil de discussion souhaitez-vous consulter ?\n");

    while(scanf("%hd", &numfil) == EOF || numfil < 0) {
      printf("Vous devez entrer le numéro du fil de discussion que vous souhaitez consulter.\n");
      scanf("%hd", &numfil);
    }
    printf("Numéro de fil : %d\n", numfil);
    numfil = htons(numfil);

    printf("Sur quel port souhaitez-vous recevoir le fichier à télécharger ?\n");

    while(scanf("%hd", &nb) == EOF || nb < 0) {
      printf("Vous devez entrer le port sur lequel le fichier sera réceptionné.\n");
      scanf("%hd", &nb);
    }
    printf("Port : %d\n", nb);
    nb = htons(nb);

    printf("Quel est le nom du fichier que vous souhaitez téléchargé ?\n");

    /*
     * SIZE_MESS - SIZE_HEADER - SIZE_NUMFIL - SIZE_NB - SIZE_O_DATALEN - sizeof(char)
     *    100    -      2      -      2      -    2    -       1        -      1        = 92
     */
    while(scanf("%92s", data) == EOF || strlen(data) <= 0) {
      printf("Vous devez entrer le nom du fichier que vous souhaitez télécharger.\n");
      scanf("%92s", data);
    }

    datalen = strlen(data);

    data[datalen] = '\0';

    printf("Nom du fichier : %s\n", data);
    printf("Taille du nom : %d\n", datalen);

    datalen = htons(datalen);

    // CREATION DU MESSAGE
    char message[SIZE_MESS];
    memset(message, 0, SIZE_MESS);

    memcpy(message, &header, SIZE_HEADER);
    memcpy(message + SIZE_HEADER, &numfil, SIZE_NUMFIL);
    memcpy(message + SIZE_HEADER + SIZE_NUMFIL, &nb, SIZE_NB);
    memcpy(message + SIZE_HEADER + SIZE_NUMFIL + SIZE_NB, &datalen, SIZE_O_DATALEN);
    memcpy(message + SIZE_HEADER + SIZE_NUMFIL + SIZE_NB + SIZE_O_DATALEN, data, datalen);

    /*** CONNEXION AU SERVEUR ***/
  
    int fdsock, adrlen;
      
    switch (connexion(hostname, port, &fdsock, &adrlen)) {
      case 0: break;
      case -1:
        fprintf(stderr, "Erreur: hote non trouve.\n");  
        exit(1);
      case -2:
        fprintf(stderr, "Erreur: echec de creation de la socket.\n");
        exit(1);
    }

    // ENVOIE DU MESSAGE
    int ecrit = send(fdsock, message, sizeof(message), 0);
    if (ecrit <= 0) {
      perror("Erreur d'envoi");
      exit(4);
    }

    // RECEPTION DU MESSAGE

    char buf[SIZE_MESS];
    memset(buf, 0, SIZE_MESS);

    int recu = recv(fdsock, buf, sizeof(buf), 0);
    if (recu < 0){
      perror("Erreur de reception");
      exit(4);
    }

    memcpy(&header, buf, SIZE_HEADER);
    header = ntohs(header);

    uint16_t codereq =  header & MASK_CODEREQ;

    if(codereq == 31) {
      printf("Le serveur à rencontrer un problème.");
    }

    close(fdsock);

    // RECEPTION DES PAQUETS

    int sock = socket(PF_INET6, SOCK_DGRAM, 0);
    if (sock < 0) {
      perror("Erreur lors de la crétion de la socket ");
      exit(-1);
    }

    struct sockaddr_in6 serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin6_family = AF_INET6;
    inet_pton(AF_INET6,"::1", &serv_addr.sin6_addr);
    serv_addr.sin6_port = htons(nb);
    socklen_t serv_addr_len = sizeof(serv_addr);

    if (bind (sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
      perror("Erreur lors du bind");
      exit(3);
    }

    char buffer[SIZE_PAQUET];
    memset(buffer, 0, sizeof(buffer));

    recu = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&serv_addr, &serv_addr_len);
    if (recu < 0) {
      perror("Erreur lors de la reception des paquets");
      exit(1);
    }

    uint16_t num_paquet, 
             taille;
    
    memcpy(&num_paquet, buffer, SIZE_NUMFIL);
    memcpy(&taille, buffer + SIZE_NUMFIL, SIZE_NB);

    num_paquet = ntohs(num_paquet);
    taille = ntohs(taille);

    while(taille != 0) {
      memset(buffer, 0, sizeof(buffer));

      recu = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&serv_addr, &serv_addr_len);
      if (recu < 0) {
        perror("Erreur lors de la reception des paquets");
        exit(1);
      }

      uint16_t num_paquet, 
              taille;
      
      memcpy(&num_paquet, buffer, SIZE_NUMFIL);
      memcpy(&taille, buffer + SIZE_NUMFIL, SIZE_NB);

      num_paquet = ntohs(num_paquet);
      taille = ntohs(taille);
    }

  }
}

/**************************************************************************************************
 *                                                MAIN                                            *
 **************************************************************************************************/

int main(int argc, char** args) {

    if (argc < 3) {
        fprintf(stderr, "Usage: %s <hostname> <port>\n", args[0]);
        exit(1);
    }

    uint16_t id = 0; // ID du client, attribue par le serveur.

    char pseudo[SIZE_PSEUDO + 1];
    memset(pseudo, 0, SIZE_PSEUDO + 1);

    int pseudo_len = 0;

    printf("Bienvenu(e) sur Mégaphone !\n");
    printf("Taper 'INSCRIPTION' pour vous inscrire, 'CONNEXION' pour vous connecter avec votre ID, ou 'AIDE' pour obtenir de l'aide !\n");

    char requete[SIZE_MESS];

    while(1) {
      
      printf("Que souhaitez-vous faire ? ");

      memset(requete, 0, SIZE_MESS);
      fflush(stdin);
      scanf("%12s", requete);

      /****** INSCRIPTION ******/
      if(strcmp(requete, CMD_INSC) == 0) { 
        if(id != 0) {
          printf("Vous êtes déjà connecté(e) !\n");
        } else {
          inscription(args[1], args[2], &id, pseudo, &pseudo_len);
        }
      /****** CONNEXION ******/
      } else if(strcmp(requete, CMD_CONN) == 0) {
        if(id != 0) {
          printf("Vous êtes déjà connecté(e) !\n");
        } else{
          id = connexion_bis();
        }
      /****** POSTER UN BILLET ******/
      } else if(strcmp(requete, CMD_POST) == 0){
        if(id == 0) {
          printf("L'identifiant n'a pas été reconnu...veuillez vous reconnecter.\n");
        } else {
          poster(args[1], args[2], &id);
        }

      /****** LISTER LES BILLETS ******/
      } else if(strcmp(requete, CMD_LIST) == 0){
        if(id == 0) {
          printf("L'identifiant est erroné...veuillez vous reconnecter.\n");
        } else {
          lister(args[1], args[2], &id);
        }

      /****** S'ABONNER A UN FIL ******/
      } else if(strcmp(requete, CMD_ABON) == 0) {
        if(id == 0) {
          printf("L'identifiant est erroné...veuillez vous reconnecter.\n");
        } else {
          abonner(args[1], args[2], args[3], &id);
        }
      
      /****** AJOUTER UN FICHIER ******/
      } else if(strcmp(requete, CMD_AJOU) == 0) {
        if(id == 0) {
          printf("L'identifiant est erroné...veuillez vous reconnecter.\n");
        } else {
          ajouter(args[1], args[2], &id);
        }

      /****** TELECHARGER UN FICHIER ******/
      } else if(strcmp(requete, CMD_TELE) == 0){
        if(id == 0) {
          printf("Vous n'êtes pas connecté(e)...\n");
        } else {
          telecharger(args[1], args[2], &id);
        }

      /****** DECONNEXION ******/
      } else if(strcmp(requete, CMD_DECO) == 0) {
        if(id == 0) {
          printf("L'identifiant est erroné...veuillez vous reconnecter.\n");
        } else {
          id = 0;
          printf("Au revoir %s !\n", pseudo);
          memset(pseudo, 0, SIZE_PSEUDO + 1);
        }

      /****** QUITTER L'APPLICATION ******/
      } else if(strcmp(requete, CMD_QUIT) == 0) {
        exit(0);

      /****** AIDE ******/
      } else if(strcmp(requete, CMD_AIDE) == 0) {
        printf("Taper :\n");
        printf("  > 'INSCRIPTION' pour vous inscrire.\n");
        printf("  > 'CONNEXION' pour vous connecter.\n");
        printf("  > 'DECONNEXION' pour vous déconnceter.\n");
        printf("  > 'QUITTER' pour quitter l'application.\n");
        printf("  > 'POSTER' pour poster un billet.\n");
        printf("  > 'LISTER' pour lister des billets d'un fil de discussion.\n");
        printf("     Si vous souhaitez consulter tous les fils de discussion, entrez '0'.\n");
        printf("     Si vous souhaitez consulter tous les billets d'un fil de discussion, entrez '0'.\n");
        printf("     Si vous souhaitez tous consulter, entrez 'LISTER 0 0'.\n");
        printf("  > 'ABONNER' pour vous abonner à un fil de discussion.\n");
        printf("  > 'AJOUTER' pour ajouter un fichier à un fil de discussion.\n");
        printf("  > 'TELECHARGER' pour télécharger un ficher d'un fil de discussion.\n");

      /****** REQUETE INCONNUE ******/
      } else {
        printf("Requête inconnue.\n");
        printf("Vous pouvez taper 'AIDE' pour accéder à la liste des requêtes possibles.\n");
      }
      printf("\n");
    }

}
