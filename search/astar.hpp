#include "../search/search.hpp"
#include "../search/closedlist.hpp"
#include "../search/openlist.hpp"
#include <boost/pool/object_pool.hpp>

template <class D, class Cost> struct AstarNode {
	ClosedEntry<AstarNode, D> closedent;
	typename D::PackedState packed;
	typename D::Oper op, pop;
	Cost g, f;
	AstarNode *parent;
	long openind;

	AstarNode(void) : openind(-1) {}

	static bool pred(AstarNode *a, AstarNode *b) {
		if (a->f == b->f)
			return a->g > b->g;
		return a->f < b->f;
	}

	static void setind(AstarNode *n, int i) { n->openind = i; }

	static int getind(AstarNode *n) { return n->openind; }
};

template <class D> struct AstarNode <D, IntOpenCost> {
	ClosedEntry<AstarNode, D> closedent;
	OpenEntry<AstarNode> openent;
	typename D::PackedState packed;
	typename D::Oper op, pop;
	typename D::Cost g, f;
	AstarNode *parent;

	static typename D::Cost prio(AstarNode *n) { return n->f; }

	static OpenEntry<AstarNode> &openentry(AstarNode *n) {
		return n->openent;
	}
};

template <class D> struct Astar : public Search<D> {

	typedef typename D::State State;
	typedef typename D::PackedState PackedState;
	typedef typename D::Undo Undo;
	typedef typename D::Cost Cost;
	typedef typename D::Oper Oper;
	typedef AstarNode<D, Cost> Node;

	Astar(int argc, const char *argv[]) :
		Search<D>(argc, argv), closed(30000001) {
		nodes = new boost::object_pool<Node>();
	}

	~Astar(void) {
		delete nodes;
	}

	Result<D> &search(D &d, typename D::State &s0) {
		Search<D>::res.start();
		closed.init(d);

		Node *n0 = init(d, s0);
		closed.add(n0);
		open.push(n0);

		while (!open.empty() && !Search<D>::limit()) {
			Node *n = open.pop();
			State buf, &state = d.unpack(buf, n->packed);

			if (d.isgoal(state)) {
				handlesol(d, n);
				break;
			}

			expand(d, n, state);
		}
		Search<D>::res.finish();

		return Search<D>::res;
	}

	virtual void reset(void) {
		Search<D>::reset();
		open.clear();
		closed.clear();
		delete nodes;
		nodes = new boost::object_pool<Node>();
	}

	virtual void output(FILE *out) {
		Search<D>::output(out);
		closed.prstats(stdout, "closed ");
		dfpair(stdout, "open list type", "%s", open.kind());
		dfpair(stdout, "node size", "%u", sizeof(Node));
	}

private:

	void expand(D &d, Node *n, State &state) {
		Search<D>::res.expd++;

		for (unsigned int i = 0; i < d.nops(state); i++) {
			Oper op = d.nthop(state, i);
			if (op == n->pop)
				continue;
			Node *k = kid(d, n, state, op);
			Search<D>::res.gend++;

			considerkid(d, k);
		}
	}

	void considerkid(D &d, Node *k) {
		unsigned long h = k->packed.hash();
		Node *dup = closed.find(k->packed, h);
		if (dup) {
			Search<D>::res.dups++;
			if (k->g >= dup->g) {
				nodes->destroy(k);
				return;
			}
			Search<D>::res.reopnd++;
			if (open.mem(dup))
				open.pre_update(dup);

			dup->f = dup->f - dup->g + k->g;
			dup->g = k->g;
			dup->op = k->op;
			dup->pop = k->pop;
			dup->parent = k->parent;

			if (!open.mem(dup))
				open.push(dup);
			else
				open.post_update(dup);
			nodes->destroy(k);
		} else {
			closed.add(k, h);
			open.push(k);
		}
	}

	Node *kid(D &d, Node *pnode, State &pstate, Oper op) {
		Node *kid = nodes->construct();
		kid->op = op;
		kid->pop = d.revop(pstate, op);
		kid->parent = pnode;
		Undo u(pstate, op);
		Cost c;
		State buf, &kidst = d.apply(buf, pstate, c, op);
		kid->g = pnode->g + c;
		kid->f = kid->g + d.h(kidst);
		d.pack(kid->packed, kidst);
		d.undo(pstate, u);

		return kid;
	}

	Node *init(D &d, State &s0) {
		Node *n0 = nodes->construct();
		d.pack(n0->packed, s0);
		n0->g = 0;
		n0->f = d.h(s0);
		n0->pop = n0->op = D::Nop;
		n0->parent = NULL;
		return n0;
	}

	void handlesol(D &d, Node *n) {
		Search<D>::res.cost = n->g;

		for ( ; n; n = n->parent) {
			State buf;
			State &state = d.unpack(buf, n->packed);
			Search<D>::res.path.push_back(state);
			if (n->parent)
				Search<D>::res.ops.push_back(n->op);
		}
	}

	struct ClosedOps {
		static PackedState &key(Node *n) { return n->packed; }
		static unsigned long hash(PackedState &s) { return s.hash(); }
		static bool eq(PackedState &a, PackedState &b) { return a.eq(b); }
		static ClosedEntry<Node, D> &entry(Node *n) { return n->closedent; }
	};

	OpenList<Node, Node, Cost> open;
 	ClosedList<ClosedOps, Node, D> closed;
	boost::object_pool<Node> *nodes;
};
