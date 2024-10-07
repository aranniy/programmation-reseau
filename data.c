#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <arpa/inet.h>

#include "data.h"

/*
 * Crée un nouvel utilisateur et l'ajoute dans la liste
 * @param[in]   users   Liste des utilisateurs répertoiriés
 * @param[in]   id      ID du nouvel utilisateur à ajouter
 * @param[in]   name    Pseudo du nouvel utilisateur à ajouter
 */
user* add_user(user* users, int id, char *name) {

    // Crée un nouvel utilisateur
    struct user *new_user = malloc(sizeof(struct user));

    if(new_user == NULL) return users;

    memset(new_user, 0, sizeof(struct  user));

    new_user -> id = id;
    strncpy(new_user -> name, name, SIZE_PSEUDO - 1);
    new_user -> next = NULL;

    // Insertion du nouveau client à la fin de la liste chaînée
    if (users == NULL) {
        return new_user;
    } else {
        struct user *p = users;
        while (p -> next != NULL) {
            p = p -> next;
        }
        p -> next = new_user;
        return users;
    }
}

/*
 * Crée un nouveau billet.
 * @param[in]   id     ID de l'auteur du billet.
 * @param[in]   data   Contenu du billet.
 * @return    Retourne un pointeur vers le nouveau billet ou NULL.
 */
struct billet * new_billet(int id, char* data) {

  struct billet * billet = malloc(sizeof(struct billet));
  
  if(billet == NULL) {
    return NULL;
  }

  memset(billet, 0, sizeof(struct billet));

  billet -> id_auteur = id;
  billet -> datalen = strlen(data);
  billet -> data = data;
  billet -> prec = NULL;

  return billet;
}

/*
 * Crée un nouveau fil de discussion.
 * @param[in]    numfil     Numero du nouveau fil.
 * @param[in]    id         ID du créateur du fil.
 * @param[in]    billet     Premier billet du fil.
 * #return    Retourne un pointeur vers le nouveau fil de discussion ou NULL.
 */
struct fil * new_fil(int numfil, int id, struct billet * billet) {

  printf("----- NEW_FILL -----\n");

  printf("ID : %d\n", id);
  printf("NUMFIL : %d\n", numfil);

  struct fil * fil = malloc(sizeof(struct fil));
  if(fil == NULL) {
    return NULL;
  }
  
  memset(fil, 0, sizeof(struct fil));

  fil -> num_fil = numfil;
  fil -> id_createur = id;
  fil -> last_billet = billet;
  fil -> nb_billets = 1;
  fil -> next = NULL;
  fil -> nb_abonnes = 0;
  fil -> port_mult = 5000 + numfil;

  fil -> addr_mult = malloc(sizeof(INET6_ADDRSTRLEN));
  if(fil -> addr_mult == NULL)
    return NULL;
  sprintf(fil -> addr_mult, "ff15::%x", numfil);

  return fil;
}

/*
 * Ajouter un billet dans un fil de discussion.
 * @param[in]   fils     Liste des fils de discussion.
 * @param[in]   data     Contenenu du billet.
 * @param[in]   id       ID de l'auteur.
 * @param[in]   numfil   Numero du fil auquel le billet sera rajoute.
 * @return    Retourne 0 en cas d'erreur,
 *            ou le numero du fil auquel le billet a ete ajoute en cas de succes.
 */
fil * add_billet(fil* fils, char* data, int id, int numfil, int* res) {

  printf("----- ADD_BILLET -----\n");

  printf("DATA : %s\n", data);
  printf("ID : %d\n", id);
  printf("NUMFIL : %d\n", numfil);
  
  billet *billet = new_billet(id, data);
  if(billet == NULL){
    printf("Erreur création du billet\n");
    *res = 0;
    return fils;
  }

  // Ajout du billet au fil souhaité

  // Dans le cas où il n'existe aucun fil
  if(fils == NULL){
    fil *fil = new_fil(1, id, billet);
    if(fil != NULL) {
      printf("Fil n°1 ajouté et billet posté !\n");
      *res = 1;
    } else {
      printf("Erreur création du fil\n");
      *res = 0;
    }
    return fil;
  }
  // Dans le cas où l'utilisateur veut créer un nouveau fil
  else if(numfil == 0) {
    struct fil * tmp = fils;
    while(tmp -> next != NULL){
      tmp = fils -> next;
    }
    numfil = tmp -> num_fil + 1;

    fil * fil = new_fil(numfil, id, billet);
    if(fil != NULL){
      tmp -> next = fil;
      printf("Fil n°%d ajouté et billet posté !\n", numfil);
      *res = numfil;
    } else {
      printf("Erreur création du fil\n");
      *res = 0;
    }
    return fils;
  }
  // Dans le cas où l'utilisateur veut ajouter un billet à un fil existant
  else {
    struct fil * tmp = fils;
    while(tmp -> next != NULL && tmp -> num_fil != numfil){
      tmp = fils -> next;
    }
    if(tmp -> num_fil != numfil){
      printf("Numéro de fil inexistant.\n");
      free(billet);
      return fils;
    } else {
      billet -> prec = tmp -> last_billet;
      tmp -> last_billet = billet;
      tmp -> nb_billets++;

      printf("Billet posté sur le fil n°%d !\n", numfil);

      *res = numfil;
      return fils;
    }
  }
}

/*
 * Cherche le nom d'un utilisateur à partir de son ID.
 * @param[in]   users    Liste des utilisateurs.
 * @param[in]   id       ID de l'utilisateurs recherché.
 * @return   Le pseudo de l'utilisateur ou NULL.
 */
char* id_to_username(user* users, uint16_t id) {
  user* user = users;
  while(user != NULL) {
    if(user -> id == id) {
      return user -> name;
    } 
    user = user -> next;
  }
  return NULL;
}

/*
 * Compte le nombre d'utilisateurs inscrits.
 * @param[in]   users   Tête de la liste des utilisateurs.
 * @return    Retourne le nombre d'utilisateurs inscrits.
 */
int number_user(user* users) {
  int cpt = 0;
  user* user = users;
  while(user != NULL) {
    cpt++;
    user = user -> next;
  }
  return cpt;
}

/*
 * Compte le nombre de fils en cours.
 * @param[in]   fils   Tête de la file de fils.
 * @return    Retourne le nombre de fils en cours.
 */
int number_fil(fil* fils) {
  int cpt = 0;
  fil* fil = fils;
  while(fil != NULL) {
    cpt++;
    fil = fil -> next;
  }
  return cpt;
}

/*
 * Recuperer un fil particulier.
 * @param[in]    fils      Liste des fils
 * @param[in]    numfil    Le numero de fil qu'on souhaite consulté
 * @return    Le fil n°<numfil> s'il existe, ou NULL sinon.
 */
fil* get_fil(fil* fils, int numfil) {
  fil* fil = fils;
  while(fil != NULL) {
    if(fil -> num_fil == numfil) {
      return fil;
    } 
    fil = fil -> next;
  }
  return fil;
}

/*
 * Recuperer un fichier particulier.
 * @param[in]    files      Liste des fichiers sur le serveur
 * @param[in]    name      Le numero de fil qu'on souhaite consulté
 * 
 * @return    Le fichier recherché ou NULL s'il existe, ou NULL sinon.
 */
fichier* get_file(fichier* files, char* name) {

  fichier* fic = files;

  while(fic != NULL) {
    if(strcmp(fic -> nom, name) == 0) {
      return fic;
    }
    fic = fic -> suivant;
  }

  return fic;
  
}