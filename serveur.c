#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>
#include <inttypes.h>
#include <net/if.h>
#include <poll.h>

#include "data.h"

static int id = 0;              // Attribution de l'id
user *utilisateurs = NULL;      // Liste des utilisateurs inscrits
fil *fils = NULL;               // Liste des fils en cours.
fichier* liste_fichiers = NULL; // Liste des fichiers en cours.


pthread_mutex_t mutex;

/**************************************************************************************************
 *                                     FONCTIONS AUXILLIAIRES                                     *
 **************************************************************************************************/

/*
 * Envoir un billet à l'utilisateur.
 * @param[in]   sockclient    Socket de communication avec le client.
 * @param[in]   fil           Fil dans lequel se trouve le billet.
 * @param[in]   billet        BIllet devant être envoyé à l'utilisateur.
 * @return    0 en cas d'échec et 1 en cas de succes.
 */
int send_billet(int sockclient, fil* fil, billet* billet) {

  printf("----- SEND_BILLET -----\n");

  char message_billet[SIZE_MESS],
       *origine = id_to_username(utilisateurs, fil -> id_createur),
       *pseudo = id_to_username(utilisateurs, billet -> id_auteur);

  uint16_t numfil = htons(fil -> num_fil);
  int datalen = billet -> datalen;

  memset(message_billet, 0, sizeof(SIZE_MESS));

  memcpy(message_billet, &numfil, SIZE_NUMFIL);
  printf("NUMFIL : %d\n", ntohs(numfil));
  memcpy(message_billet + SIZE_NUMFIL, origine, SIZE_PSEUDO);
  printf("ORIGINE : %s\n", origine);
  memcpy(message_billet + SIZE_NUMFIL + SIZE_PSEUDO, pseudo, SIZE_PSEUDO);
  printf("PSEUDO : %s\n", pseudo);
  memcpy(message_billet + SIZE_NUMFIL + SIZE_PSEUDO + SIZE_PSEUDO, &datalen, SIZE_O_DATALEN);
  printf("DATALEN : %d\n", datalen);
  memcpy(message_billet + SIZE_NUMFIL + SIZE_PSEUDO + SIZE_PSEUDO + SIZE_O_DATALEN, billet -> data, datalen);
  printf("DATA : %s\n", billet -> data);

  if(send(sockclient, message_billet, SIZE_NUMFIL + SIZE_PSEUDO + SIZE_PSEUDO + SIZE_O_DATALEN + datalen, 0) < 0) {
    printf("ECHEC\n");
    return 0;
  }
  printf("SUCCES\n");
  return 1;
}

int compare_packets(const void *a, const void *b) {
    struct Packet *packet1 = (struct Packet *)a;
    struct Packet *packet2 = (struct Packet *)b;
    return packet1->numbloc - packet2->numbloc;
}

/**************************************************************************************************
 *                                          COMMANDES                                             *
 **************************************************************************************************/

/*
 * Inscription d'un nouvel utilisateur
 * @param[in]    sockclient    Socket pour communiquer avec l'utilisateur
 * @param[in]    pseudo        Pseudo de l'utilisateur
 */
void inscription(int sockclient, char* pseudo) {

  printf("----- INSCRIPTION -----\n");

  // WARNING : pour l'instant on accepte tous les pseudo lol (on doit attendre les non bloquants)

    // Ajouter le client à la liste des inscrits 
    pthread_mutex_lock(&mutex);
    id++;
    utilisateurs = add_user(utilisateurs, id, pseudo);
    pthread_mutex_unlock(&mutex);

    printf("Inscription effectuée !\n");
    printf("Pseudo : %s\n", pseudo);
    printf("ID : %d\n", id);

    /*** Envoie d'un message ***/
    char buf_message[SIZE_MESS];
    memset(buf_message, 0, SIZE_MESS);

    uint16_t header = htons((id << SIZE_CODEREQ) | CODEREQ_INSC);

    memcpy(buf_message, &header, SIZE_HEADER);

    int ecrit = send(sockclient, buf_message, sizeof(buf_message), 0);
    if(ecrit <= 0){
      perror("Erreur envoi\n");
      exit(3);
    }

  printf("NUMBER OF USERS : %d\n", number_user(utilisateurs));

  if(utilisateurs == NULL) printf("ERREUR : Pas d'utilisateurs...\n");

}

/*
 * Poster un billet.
 * @param[in]    sockclient    Socket de communication avec le client.
 * @param[in]    numfil        Numero du fil de discussion auquel sera rajouté le billet.
 * @param[in]    id_createur   ID de l'auteur du billet.
 * @param[in]    datalen       Taille du billet.
 * @param[in]    buf           Contenu du billet.
 */
void poster(int sockclient, int numfil, int id_createur, int datalen, char* buf){

  printf("\n----- POSTER -----\n");
  
  uint16_t header;
  uint16_t new_numfil = 0;

  printf("NUMFIL : %d\n", numfil);
  printf("ID : %d\n", id_createur);
  printf("Datalen : %d\n", datalen);
  printf("Data : %s\n", buf);

  if(id_createur > id){
    header = htons((id << SIZE_CODEREQ) | CODEREQ_ERRE);
    printf("Identifiant inconnu.\n");
  } else {
    int res = 0;
    fils = add_billet(fils, buf, id_createur, numfil, &res);
    if(res == 0){
      header = htons((id << SIZE_CODEREQ) | CODEREQ_ERRE);
    } else {
      header = htons((id << SIZE_CODEREQ) | CODEREQ_POST);
       new_numfil = htons(res);
    }
  }

  char message[SIZE_MESS];
  memset(message, 0, SIZE_MESS);
  memcpy(message, &header, SIZE_HEADER);
  memcpy(message + SIZE_HEADER, &new_numfil, SIZE_NUMFIL);

  int ecrit = send(sockclient, message, sizeof(message), 0);
  if(ecrit <= 0){
    perror("erreur ecriture");
    exit(3);
  }

  printf("NOMBRE DE FILS : %d\n", number_fil(fils));

  if(fils == NULL) printf("ERREUR : Pas de fil de discussion...\n");

}

void send_erreur(int socketclient){
  uint16_t header = htons(CODEREQ_ERRE);
  char message[SIZE_HEADER + SIZE_NUMFIL + SIZE_NB];
  memcpy(message, &header, SIZE_HEADER);
  if(send(socketclient, message, sizeof(message), 0) <= 0)
    perror("Erreur envoie ");
}

/*
 * Lister les billets qu'un utilisateur souhaite consulter.
 * @param[in]   socketclient    Socket pour communiquer avec l'utilisateur.
 * @param[in]   id              ID de l'utilisateur.
 * @param[in]   f               Numéro de la fil que l'utilisateur souhaite consulté.
 * @param[in]   n               Nombre de billets que l'utilisateur souhaite consulté.
 */
void lister(int socketclient, uint16_t id, uint16_t f, uint16_t n) {

  printf("\n----- LISTER -----\n");
  printf("ID : %d\nF : %d\nN : %d\n", id, f, n);

  /**** CHAMPS ****/
  uint16_t header,
           numfil,
           nb;

  char origine[SIZE_PSEUDO],
       pseudo[SIZE_PSEUDO],
       message[SIZE_HEADER + SIZE_NUMFIL + SIZE_NB];
       
  memset(origine, 0, SIZE_PSEUDO);
  memset(pseudo, 0, SIZE_PSEUDO);
  memset(message, 0, sizeof(message));

  /***** PREMIER MESSAGE *****/

  printf("PREMIER MESSAGE\n");

  // Initialisation de l'en-tête
  header = htons((id << SIZE_CODEREQ) | CODEREQ_LIST);

  // Initialisation de <numfil>

  // On demande un fil en particulier
  if(f > 0) { 
    numfil = f;
  // On demande à consulter tous les fils
  } else if(f == 0) {
    numfil = number_fil(fils);
  // Le numero de fil est négatif
  } else {
    send_erreur(socketclient);
    return;
  } 
  printf("NUMFIL: %d\n", numfil);
  numfil = htons(numfil);

  // Initialisation de <nb>

  // On consulte un fil particulier
  if(f > 0) { 
    fil* fil = get_fil(fils, f);
    // On ne trouve pas le fil
    if(fil == NULL) {
      send_erreur(socketclient);
      return;
    }
    // Nombre de billets positif
    if(n >= 0) { 
      // Le nombre de billets demande est inferieur ou égale aux nombres de billets dans le fil
      if(n <= fil -> nb_billets) {
        nb = n;
      // Le nombre de billet demande est plus grand que le nombre de billets dans le fil
      } else {
        nb = fil -> nb_billets;
      }
    // Nombre de billets negatif
    } else {
      send_erreur(socketclient);
      return;
    }
  // On veut consulter tous les fils
  } else { 
    fil* fil = fils;
    nb = 0;
    // On veut consulter tous les billets
    if(n == 0) {
      while(fil != NULL) {
        nb += fil -> nb_billets;
        fil = fil -> next;
      }
    // On veut consulter un nombre de billets particulier
    } else {
      // Nombre de billets negatif
      if(n < 0) {
        send_erreur(socketclient);
        return;
      }
      while(fil != NULL) {
        int nb_billets = fil -> nb_billets;
        printf("NB_BILLETS : %d\n", nb_billets);
        // On demande moins ou autant de billets qu'il y en a dans le fil
        if(n <= nb_billets) {
          nb += n;
        // On demande plus de billet que ce qu'il y en a dans le fil
        } else {
          nb = nb_billets;
        }
        fil = fil -> next;
      }
    }
  }
  printf("NB: %d\n", nb);
  nb = htons(nb);

  // Creation et envoie du premier message
  memcpy(message, &header, SIZE_HEADER);
  memcpy(message + SIZE_HEADER, &numfil, SIZE_NUMFIL);
  memcpy(message + SIZE_HEADER + SIZE_NUMFIL, &nb, SIZE_NB);

  if(send(socketclient, message, sizeof(message), 0) <= 0) {
    perror("Erreur envoie ");
    return;
  }

  memset(message, 0, sizeof(message));

  /***** AUTRES MESSAGES *****/

  printf("AUTRES MESSAGES\n");
  
  // Le client veut consulter tous les fils
  if(f == 0) {

    fil* fil = fils;

    // Le client veut consulter tous les billets
    if(n == 0) {

      while(fil != NULL) {
        billet* billet = fil -> last_billet;

        while(billet != NULL) {
          if(send_billet(socketclient, fil, billet) <= 0) {
            send_erreur(socketclient);
            return;
          }
          billet = billet -> prec;
        }

        fil = fil -> next;
      }

    // Le client veut un certain nombre de billet
    } else {

      while(fil != NULL) {
        billet* billet = fil -> last_billet;
        int nb_billets;
        // Le fil contient plus ou autant de billets que ce que le client a demande
        if(n <= fil -> nb_billets) {
          nb_billets = n;
        // Le fil contient moins de billets que ce que le client a demande
        } else {
          nb_billets = fil -> nb_billets;
        }

        for(int i = 0; i < nb_billets; i++) {
          if(send_billet(socketclient, fil, billet) <= 0) {
            send_erreur(socketclient);
            return;
          }
          billet = billet -> prec;
        }
        fil = fil -> next;
      }

    }

  // Le client veut consulter un fil en particulier
  } else {

    fil* fil = get_fil(fils, f);
    billet* billet = fil -> last_billet;

    // Le client veut consulter tous les billets
    if(n == 0) {

      while(billet != NULL) {
        if(send_billet(socketclient, fil, billet) <= 0) {
          send_erreur(socketclient);
          return;
        }
        billet = billet -> prec;
      }

    // Le client veut consulter un certain nombre de billets
    } else {
      int nb_billets;
      // Le fil contient plus ou autant de billets que ce que le client a demande
      if(n <= fil -> nb_billets) {
        nb_billets = n;
      // Le fil contient moins de billets que ce que le client a demande
      } else {
        nb_billets = fil -> nb_billets;
      }

      for(int i = 0; i < nb_billets; i++) {
        if(send_billet(socketclient, fil, billet) <= 0) {
          send_erreur(socketclient);
          return;
        }
        billet = billet -> prec;
      }
    }
  }

}

void abonner(int sockclient, int id_createur, int nf){
  printf("\n----- ABONNER -----\n");

  if(id_createur > id){
    send_erreur(sockclient);
    return;
  }

  fil * tmp = get_fil(fils, nf);
  tmp -> nb_abonnes++;

  printf("NUMFIL : %d\n", nf);
  printf("ID : %d\n", id);
  printf("PORT : %d\n", tmp -> port_mult);
  printf("ADRESSE IPv6: %s\n", tmp -> addr_mult);

  uint16_t header = htons((id_createur << SIZE_CODEREQ) | CODEREQ_ABON);
  uint16_t numfil = htons(nf);
  uint16_t port = htons(tmp -> port_mult);

  char adresse_str[SIZE_ADDRMULT];
  if (inet_ntop(AF_INET6, &tmp -> addr_mult, adresse_str, SIZE_ADDRMULT) == NULL) {
    perror("Erreur conversion de l'adresse IPv6");
    return;
  }

  char message[SIZE_MESS];
  memset(message, 0, SIZE_MESS);
  memcpy(message, &header, SIZE_HEADER);
  memcpy(message + SIZE_HEADER, &numfil, SIZE_NUMFIL);
  memcpy(message + SIZE_HEADER + SIZE_NUMFIL, &port, SIZE_NB);
  memcpy(message + SIZE_HEADER + SIZE_NUMFIL + SIZE_NB, &adresse_str, SIZE_ADDRMULT);

  int ecrit = send(sockclient, message, sizeof(message), 0);
  if(ecrit <= 0){
    perror("erreur ecriture");
    exit(3);
  }
}

void notification(){
  /* Code provenant du cours que nous aurions pu utiliser pour les notifications
    if((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) return 1;
    // Initialisation de l’adresse d’abonnement
    struct sockaddr_in grsock;
    memset(&grsock, 0, sizeof(grsock));
    grsock.sin_family = AF_INET;
    inet_pton(AF_INET, "225.1.2.3", &grsock.sin_addr);
    grsock.sin_port = htons(4321);
    struct ip_mreqn group;
    memset(&group, 0, sizeof(group));
    group.imr_multiaddr.s_addr = htonl(INADDR_ANY);
    group.imr_ifindex = if_nametoindex ("eth0");
    if(setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF, &group, sizeof(group))){
      perror("erreur initialisation de l’interface locale");
      return 1;
    }
  */
}

void ajouter(int sockclient, int numfil, int id_createur, int datalen, char* buf) {
  uint16_t header;
  uint16_t nb;
  if(id_createur > id){
    header = htons((id << SIZE_CODEREQ) | CODEREQ_ERRE);
    printf("Identifiant inconnu.\n");
  } else {
    header = htons((id << SIZE_CODEREQ) | CODEREQ_AJOU);
  }

  // formation du message à envoyer
  nb = htons(4930); 
  char message[SIZE_MESS];
  memset(message, 0, SIZE_MESS);
  memcpy(message, &header, SIZE_HEADER);
  memcpy(message + SIZE_HEADER, &numfil, SIZE_NUMFIL);
  memcpy(message + SIZE_HEADER + SIZE_NUMFIL, &nb, SIZE_NB);

  // envoi du message
  int ecrit = send(sockclient, message, sizeof(message), 0);
  if(ecrit <= 0){
    perror("erreur ecriture");
    exit(3);
  }

  // connecter au port udp
  int sock_udp = socket(PF_INET6, SOCK_DGRAM, 0);
  if (sock_udp < 0) {
    perror("Erreur lors de la création de la socket");
    exit(-1);
  }

  struct sockaddr_in6 servadr;
  memset(&servadr, 0, sizeof(servadr));
  servadr.sin6_family = AF_INET6;
  servadr.sin6_addr = in6addr_any;
  servadr.sin6_port = htons(4930);

  if (bind(sock_udp, (struct sockaddr *)&servadr, sizeof(servadr)) < 0) {
    perror("Erreur lors du bind");
    exit(3);
  }

  size_t size = 1 << 25; // 2 puissance 25 octets
  char* buffer = malloc(size);

  if (buffer == NULL) {
    perror("Erreur lors de l'allocation mémoire");
    exit(1);
  }

  int timeout = (TIMEOUT_SEC * 1000 + TIMEOUT_USEC) / 1000;
  struct pollfd fds[1];
  fds[0].fd = sock_udp;
  fds[0].events = POLLIN;

  int ret = poll(fds, 1, timeout);
  if (ret == -1) {
    perror("Erreur lors de l'appel à poll");
    exit(3);
  } else if (ret == 0) {
    printf("Aucune donnée reçue sur le port UDP\n");
    exit(3);
  }

  // Variables pour le suivi des paquets
  struct Packet* packets = NULL;  // Liste chaînée des paquets
  int packet_count = 0;           // Nombre de paquets reçus
  int length = 0;                 // Longueur totale des données réassemblées

  if (fds[0].revents & POLLIN) {
    while (1) {
      socklen_t len = sizeof(servadr);
      int rcv = recvfrom(sock_udp, buffer, size, 0, (struct sockaddr *)&servadr, &len);
      if (rcv < 0) {
        perror("recvfrom");
        exit(1);
      } else if (rcv < SIZE_PAQUET) {
        break; // dernier paquet
      }
          
      // Récupération du numéro de bloc du paquet
      int numbloc;
      memcpy(&numbloc, buffer + SIZE_HEADER, SIZE_NUMFIL);

      // Stockage du paquet dans la structure de données
      struct Packet* new_packet = malloc(sizeof(struct Packet));
      if (new_packet == NULL) {
        perror("Erreur lors de l'allocation mémoire");
        exit(1);
      }

      new_packet->numbloc = numbloc;
      new_packet->taille = rcv - SIZE_HEADER - SIZE_NUMFIL;
      new_packet->data = malloc(sizeof(char) * new_packet->taille);
      if (new_packet->data == NULL) {
        perror("Erreur lors de l'allocation mémoire");
        exit(1);
      }
      memcpy(new_packet->data, buffer + SIZE_HEADER + SIZE_NUMFIL, new_packet->taille);
      new_packet->next = NULL;

      // Ajouter le paquet à la liste chaînée des paquets
      if (packets == NULL) {
        packets = new_packet;  // Premier paquet de la liste
      } else {
        struct Packet* current = packets;
        while (current->next != NULL) {
          current = current->next;
        }
        current->next = new_packet;  // Ajouter le paquet à la fin de la liste
      }

      packet_count++;
      length += new_packet->taille;
    }

    // Conversion de la liste chaînée en tableau pour le tri
    struct Packet** packet_array = malloc(sizeof(struct Packet*) * packet_count);
    if (packet_array == NULL) {
      perror("Erreur lors de l'allocation mémoire");
      exit(1);
    }

    struct Packet* current_packet = packets;
    int i = 0;
    while (current_packet != NULL) {
      packet_array[i] = current_packet;
      current_packet = current_packet->next;
      i++;
    }

    // Tri du tableau de paquets en fonction du numéro de bloc
    qsort(packet_array, packet_count, sizeof(struct Packet*), compare_packets);

    // Réassemblage des données dans la structure Fichier
    struct Fichier* nouveau_fichier = malloc(sizeof(struct Fichier));
    if (nouveau_fichier == NULL) {
      perror("Erreur lors de l'allocation mémoire");
      exit(1);
    }

    strcpy(nouveau_fichier->nom, buf);
    nouveau_fichier->taille = length;
    nouveau_fichier->donnees = malloc(length);
    if (nouveau_fichier->donnees == NULL) {
      perror("Erreur lors de l'allocation mémoire");
      exit(1);
    }

    size_t offset = 0;
    for (int i = 0; i < packet_count; i++) {
      struct Packet* current_packet = packet_array[i];
      memcpy(nouveau_fichier->donnees + offset, current_packet->data, current_packet->taille);
      offset += current_packet->taille;
      free(current_packet->data);
      free(current_packet);
    }

    free(packet_array);

    nouveau_fichier->suivant = NULL;

    // Ajouter le fichier à la liste chaînée des fichiers
    if (liste_fichiers == NULL) {
      liste_fichiers = nouveau_fichier;  // Premier fichier de la liste
    } else {
      struct Fichier* current = liste_fichiers;
      while (current->suivant != NULL) {
        current = current->suivant;
      }
      current->suivant = nouveau_fichier;  // Ajouter le fichier à la fin de la liste
    }
  }

  // Déclaration du buffer pour le contenu du billet
  char name_file[SIZE_MESS];

  // Conversion de la taille du fichier en chaîne de caractères
  char taille_str[20]; // Taille maximale de la chaîne pour la taille du fichier
  sprintf(taille_str, "%d", length);

  // Formatage du billet contenant uniquement le nom du fichier et sa taille
  snprintf(name_file, SIZE_MESS, "%s %s", buf, taille_str);

  poster(sockclient, numfil, id, datalen, name_file);

  printf("Le fichier a bien été ajouté.\n");

  free(buffer);
  close(sock_udp);
}

/*
 * Télécharger un fichier.
 *
 * @param[in]   socketclient    Socket pour communiquer avec l'utilisateur.
 * @param[in]   id              ID de l'utilisateur.
 * @param[in]   f               Numéro du fil d'où le fichier sera téléchargé.
 * @param[in]   n               Numéro du port de réception du client.
 * @param[in]   data            Nom du fichier à télécharger.
 */
void telecharger(int socketclient, uint16_t id, uint16_t f, uint16_t n, char* data) {

  printf("\n----- TELECHARGER -----\n");
  printf("ID : %d\nF : %d\nN : %d\nNOM : %s\n", id, f, n, data);
  
  fichier *fic = get_file(liste_fichiers, data);

  /**** PREMIER MESSAGE ****/

  // Initialisation du message
  char message[SIZE_HEADER + SIZE_NUMFIL + SIZE_NB];
  memset(message, 0, sizeof(message));
  
  // Si le fichier existe
  uint16_t header;

  if(fic != NULL) {

    header = htons((id << SIZE_CODEREQ) | CODEREQ_TELE);
    memcpy(message, &header, SIZE_HEADER);
    memcpy(message + SIZE_HEADER, &f, SIZE_NUMFIL);
    memcpy(message + SIZE_HEADER + SIZE_NUMFIL, &n, SIZE_NB);

  // Si le fichier n'existe pas
  } else {
    header = htons((id << SIZE_CODEREQ) | CODEREQ_ERRE);
    memcpy(message, &header, SIZE_HEADER);
  }

  // Envoie du message
  int ecrit = send(socketclient, message, sizeof(message), 0);
  if(ecrit <= 0){
    perror("erreur ecriture");
    exit(3);
  }

  /**** ENVOIE DU FICHIER ****/

  int nbr_paquets = (fic -> taille / (SIZE_PAQUET - sizeof(int))) + 1;

  // CONNECTION UDP
  int sock = socket(PF_INET6, SOCK_DGRAM, 0);

  if (sock < 0) {
    perror("Erreur lors de la création de la socket");
    exit(-1);
  }

  struct sockaddr_in6 adr;
  memset(&adr, 0, sizeof(adr));
  adr.sin6_family = AF_INET6;
  adr.sin6_addr = in6addr_any;
  adr.sin6_port = htons(n);

  // ENVOIE DES PAQUETS

  for(int i = 0; i < nbr_paquets; i++) {

    printf("PAQUET n°%d\n", i);

    char paquet[SIZE_PAQUET];
    memset(paquet, 0, sizeof(paquet));

    int size_data = SIZE_PAQUET - SIZE_NUMFIL - SIZE_NB;

    uint16_t num_paquet = htons(i);
    memcpy(paquet, &num_paquet, sizeof(SIZE_NUMFIL));

    if(size_data * i <= fic -> taille) {

      int taille;
      if(size_data * i < fic -> taille) {
        taille = size_data;
      } else {
        taille = (size_data * i) - fic -> taille;
      }

      uint16_t datalen = htons(taille);

      memcpy(paquet + SIZE_NUMFIL, &datalen, SIZE_NB);
      memcpy(paquet + SIZE_NUMFIL + SIZE_NB, fic -> donnees + (size_data * i), taille);
    } 

    int send = sendto(socketclient, paquet, SIZE_PAQUET, 0, (struct sockaddr *)&adr, sizeof(adr));
    if (send < 0) {
      perror("Erreur lors de l'envoi du paquet");
      exit(1);
    }

  }

  close(sock);
    
}

/**************************************************************************************************
 *                                     GESTION DES COMMANDES                                      *
 **************************************************************************************************/

void* client_thread(void *arg) {

  int sockclient = *(int*) arg;
  free(arg);

  if (sockclient >= 0) {

    //*** Reception d'un message ***/
    char buf[SIZE_MESS + 1];
    memset(buf, 0, SIZE_MESS + 1);
    int recu = recv(sockclient, buf, SIZE_MESS, 0);
    if (recu <= 0){
      perror("Erreur de réception ");
      exit(4);
    }
    buf[recu] = '\0';

    uint16_t header;
    memcpy(&header, buf, SIZE_HEADER);
    header = ntohs(header);
    uint16_t codereq = header & MASK_CODEREQ;

    /****** INSCRIPTION ******/
    if(codereq == CODEREQ_INSC) {
      char pseudo[SIZE_PSEUDO];
      memset(pseudo, 0, SIZE_PSEUDO);
      memcpy(pseudo, buf + SIZE_HEADER, SIZE_PSEUDO);
      inscription(sockclient, pseudo);

    /****** POSTER ******/
    } else if(codereq == CODEREQ_POST) { 
      int id = header >> SIZE_CODEREQ;

      int numfil;
      memcpy(&numfil, buf + SIZE_HEADER, SIZE_NUMFIL);
      numfil = ntohs(numfil);

      int datalen = buf[SIZE_HEADER + SIZE_NUMFIL + SIZE_NB];

      char * data = malloc(datalen);
      memcpy(data, buf + SIZE_HEADER + SIZE_NUMFIL + SIZE_NB + SIZE_O_DATALEN, datalen);

      poster(sockclient, numfil, id, datalen, data);
    
    /****** LISTER ******/
    } else if(codereq == CODEREQ_LIST) { 

      uint16_t id = header >> SIZE_CODEREQ;

      uint16_t numfil;
      memcpy(&numfil, buf + SIZE_HEADER, SIZE_NUMFIL);
      numfil = ntohs(numfil);

      uint16_t nb;
      memcpy(&nb, buf + SIZE_HEADER + SIZE_NUMFIL, SIZE_NB);
      nb = ntohs(nb);

      lister(sockclient, id, numfil, nb);

    /****** ABONNER ******/
    } else if(codereq == CODEREQ_ABON) { 

      uint16_t id = header >> SIZE_CODEREQ;

      uint16_t numfil;
      memcpy(&numfil, buf + SIZE_HEADER, SIZE_NUMFIL);
      numfil = ntohs(numfil);

      abonner(sockclient, id, numfil);

    /****** AJOUTER ******/
    } else if(codereq == CODEREQ_AJOU) {

      int id = header >> SIZE_CODEREQ;
      int numfil;
      memcpy(&numfil, buf + SIZE_HEADER,SIZE_NUMFIL);
      numfil = ntohs(numfil);
      int datalen = buf[SIZE_HEADER + SIZE_NUMFIL + SIZE_NB];

      char * data = malloc(datalen);
      memcpy(data, buf + SIZE_HEADER + SIZE_NUMFIL + SIZE_NB + SIZE_O_DATALEN, datalen);

      ajouter(sockclient,numfil,id,datalen,data);

    /****** TELECHARGER ******/
    } else if(codereq == CODEREQ_TELE) {

      uint16_t id = header >> SIZE_CODEREQ;

      uint16_t numfil;
      memcpy(&numfil, buf + SIZE_HEADER, SIZE_NUMFIL);
      numfil = ntohs(numfil);

      uint16_t nb;
      memcpy(&nb, buf + SIZE_HEADER + SIZE_NUMFIL, SIZE_NB);
      nb = ntohs(nb);

      uint16_t datalen;
      memcpy(&datalen, buf + SIZE_HEADER + SIZE_NUMFIL + SIZE_NB, SIZE_O_DATALEN);
      datalen = ntohs(datalen);

      char data[datalen];
      memset(&data, 0, sizeof(data));

      telecharger(sockclient, id, numfil, nb, data);

    /****** REQUETE INCONNUE ******/
    } else {
      char message[SIZE_MESS];
      memset(message, 0, sizeof(message));
      snprintf(message, SIZE_MESS, "Code de requête inconnue...\n");

      if(send(sockclient, message, sizeof(message), 0) <= 0) {
        perror("Erreur envoie ");
        exit(3);
      }
    }
  }

  close(sockclient);
  return NULL;

}

/**************************************************************************************************
 *                                              MAIN                                              *
 **************************************************************************************************/

int main(int argc, char *argv[]){

  if(argc != 3){
    fprintf(stderr, "usage : ./serveur <PORT> <INTERFACE MULTICAST>\n");
    exit(1);
  }

  /*********************
   *        TCP        *
   *********************/
    
  /*** Creation de l'adresse du destinataire (serveur) ***/
  struct sockaddr_in6 address_sock;
  address_sock.sin6_family = AF_INET6;
  address_sock.sin6_port = htons(atoi(argv[1]));
  address_sock.sin6_addr = in6addr_any;

  /*** Creation de la socket ***/
  int sock_tcp = socket(PF_INET6, SOCK_STREAM, 0);
  if(sock_tcp < 0){
    perror("Erreur lors de la creation de la socket du serveur ");
    exit(1);
  }

  /*** Desactiver l'option n'acceptant que de l'IPv6 ***/
  int optval = 0;
  int r = setsockopt(sock_tcp, IPPROTO_IPV6, IPV6_V6ONLY, &optval, sizeof(optval));
  if (r < 0) 
    perror("Erreur - connexion IPv4 impossible ");

  /*** Le numero de port peut etre utilise en parallele ***/
  optval = 1;
  r = setsockopt(sock_tcp, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
  if (r < 0) 
    perror("Erreur - réutilisation de port impossible ");

  /*** Lier la socket au port ***/
  r = bind(sock_tcp, (struct sockaddr *) &address_sock, sizeof(struct sockaddr_in6));
  if (r < 0) {
    perror("Erreur lors de la liaison de la socket serveur à son port ");
    exit(2);
  }

  /*** Le serveur est pret a ecouter les connexions sur le port ***/
  r = listen(sock_tcp, 0);
  if (r < 0) {
    perror("Erreur lors du démarrage du serveur ");
    exit(2);
  }

  /*********************
   *        UDP        *
   *********************/

  // Création d'une socket UDP
  int sock_udp = socket(PF_INET6, SOCK_DGRAM, 0);
  if(sock_udp < 0) {
      perror("Error while creating socket ");
      exit(-1);
  }

  // Initiaisation de l'adresse multicast
  struct sockaddr_in6 group_sock;
  memset(&group_sock, 0, sizeof(group_sock));
  group_sock.sin6_family = AF_INET6;
  inet_pton(AF_INET6, "::1", &group_sock.sin6_addr);
  group_sock.sin6_port = htons(atoi(argv[1]));

  // Récuperation de l'adresse locale de l'insterface autorisant le multicast
  int ifindex = if_nametoindex(argv[2]);

  // Initialisation de l'interface locale
  if(setsockopt(sock_udp, IPPROTO_IPV6, IPV6_MULTICAST_IF, &ifindex, sizeof(ifindex)) < 0) {
      perror("erreur initialisation de l’interface locale");
      return -1;
  }

  pthread_t threads[20];
  int nb_threads = 0;

  while (1) {

    /*** Le serveur accepte une connexion et cree la socket de communication avec le client ***/
    struct sockaddr_in6 adrclient;
    memset(&adrclient, 0, sizeof(struct sockaddr_in6));
    socklen_t size = sizeof(adrclient);
    int sockclient = accept(sock_tcp, (struct sockaddr *) &adrclient, &size);

    int* arg = malloc(sizeof(int));
    *arg = sockclient;

    pthread_t thread;
    pthread_create(&thread, NULL, client_thread, arg);

    threads[nb_threads++] = thread;

  }

  for (int i = 0; i < nb_threads; i++) {
    pthread_join(threads[i], NULL);
  }

  /*** Fermeture socket serveur ***/
  close(sock_tcp);
  close(sock_udp);

  return 0;
}