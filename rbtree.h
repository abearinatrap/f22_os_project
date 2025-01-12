/** C implementation for
	Red-Black Tree Insertion
	This code is provided by
	costheta_z

	https://www.geeksforgeeks.org/c-program-red-black-tree-insertion/

	(edited struct to fit purpose)
	 **/
#include <stdlib.h>
#include <pthread.h>
// Structure to represent each
// node in a red-black tree
struct node {
	int d; // data
    pthread_cond_t *rdy;
    pthread_mutex_t *mut;
    meeting_response_buf **res;
	int c; // 1-red, 0-black
	struct node* p; // parent
	struct node* r; // right-child
	struct node* l; // left child
};

// global root for the entire tree
extern struct node* root;
//struct node* root = NULL;

// function to perform BST insertion of a node
struct node* bst(struct node* trav,
					struct node* temp)
{
	// If the tree is empty,
	// return a new node
	if (trav == NULL)
		return temp;
    
	// Otherwise recur down the tree
	if (temp->d < trav->d)
	{
		trav->l = bst(trav->l, temp);
		trav->l->p = trav;
	}
	else if (temp->d > trav->d)
	{
		trav->r = bst(trav->r, temp);
		trav->r->p = trav;
	}

	// Return the (unchanged) node pointer
	return trav;
}

// Function performing right rotation
// of the passed node
void rightrotate(struct node* temp)
{
	struct node* left = temp->l;
	temp->l = left->r;
	if (temp->l)
		temp->l->p = temp;
	left->p = temp->p;
	if (!temp->p)
		root = left;
	else if (temp == temp->p->l)
		temp->p->l = left;
	else
		temp->p->r = left;
	left->r = temp;
	temp->p = left;
}

// Function performing left rotation
// of the passed node
void leftrotate(struct node* temp)
{
	struct node* right = temp->r;
	temp->r = right->l;
	if (temp->r)
		temp->r->p = temp;
	right->p = temp->p;
	if (!temp->p)
		root = right;
	else if (temp == temp->p->l)
		temp->p->l = right;
	else
		temp->p->r = right;
	right->l = temp;
	temp->p = right;
}

// This function fixes violations
// caused by BST insertion
void fixup(struct node* root, struct node* pt)
{
	struct node* parent_pt = NULL;
	struct node* grand_parent_pt = NULL;

	while ((pt != root) && (pt->c != 0)
		&& (pt->p->c == 1))
	{
		parent_pt = pt->p;
		grand_parent_pt = pt->p->p;

		/* Case : A
			Parent of pt is left child
			of Grand-parent of
		pt */
		if (parent_pt == grand_parent_pt->l)
		{

			struct node* uncle_pt = grand_parent_pt->r;

			/* Case : 1
				The uncle of pt is also red
				Only Recoloring required */
			if (uncle_pt != NULL && uncle_pt->c == 1)
			{
				grand_parent_pt->c = 1;
				parent_pt->c = 0;
				uncle_pt->c = 0;
				pt = grand_parent_pt;
			}

			else {

				/* Case : 2
					pt is right child of its parent
					Left-rotation required */
				if (pt == parent_pt->r) {
					leftrotate(parent_pt);
					pt = parent_pt;
					parent_pt = pt->p;
				}

				/* Case : 3
					pt is left child of its parent
					Right-rotation required */
				rightrotate(grand_parent_pt);
				int t = parent_pt->c;
				parent_pt->c = grand_parent_pt->c;
				grand_parent_pt->c = t;
				pt = parent_pt;
			}
		}

		/* Case : B
			Parent of pt is right
			child of Grand-parent of
		pt */
		else {
			struct node* uncle_pt = grand_parent_pt->l;

			/* Case : 1
				The uncle of pt is also red
				Only Recoloring required */
			if ((uncle_pt != NULL) && (uncle_pt->c == 1))
			{
				grand_parent_pt->c = 1;
				parent_pt->c = 0;
				uncle_pt->c = 0;
				pt = grand_parent_pt;
			}
			else {
				/* Case : 2
				pt is left child of its parent
				Right-rotation required */
				if (pt == parent_pt->l) {
					rightrotate(parent_pt);
					pt = parent_pt;
					parent_pt = pt->p;
				}

				/* Case : 3
					pt is right child of its parent
					Left-rotation required */
				leftrotate(grand_parent_pt);
				int t = parent_pt->c;
				parent_pt->c = grand_parent_pt->c;
				grand_parent_pt->c = t;
				pt = parent_pt;
			}
		}
	}

	root->c = 0;
}


