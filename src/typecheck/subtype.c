#include "subtype.h"
#include "reify.h"
#include "typechecker.h"
#include <assert.h>

static bool is_typedef_sub_typedef(ast_t* sub, ast_t* super);

static bool is_eq_typeargs(ast_t* a, ast_t* b)
{
  assert(ast_id(a) == TK_NOMINAL);
  assert(ast_id(b) == TK_NOMINAL);

  // check typeargs are the same
  ast_t* a_arg = ast_child(ast_childidx(a, 1));
  ast_t* b_arg = ast_child(ast_childidx(b, 1));

  while((a_arg != NULL) && (b_arg != NULL))
  {
    if(!is_eqtype(a_arg, b_arg))
      return false;

    a_arg = ast_sibling(a_arg);
    b_arg = ast_sibling(b_arg);
  }

  // make sure we had the same number of typeargs
  return (a_arg == NULL) && (b_arg == NULL);
}

static bool is_function_sub_function(ast_t* sub, ast_t* super)
{
  // TODO:
  return false;
}

static bool is_structural_sub_function(ast_t* sub, ast_t* fun)
{
  // must have some function that is a subtype of fun
  ast_t* sub_fun = ast_child(sub);

  while(sub_fun != NULL)
  {
    if(is_function_sub_function(sub_fun, fun))
      return true;

    sub_fun = ast_sibling(sub_fun);
  }

  return false;
}

static bool is_structural_sub_structural(ast_t* sub, ast_t* super)
{
  // must be a subtype of every function in super
  ast_t* fun = ast_child(super);

  while(fun != NULL)
  {
    if(!is_structural_sub_function(sub, fun))
      return false;

    fun = ast_sibling(fun);
  }

  return true;
}

static bool is_functiondef_sub_function(ast_t* sub, ast_t* fun)
{
  // TODO:
  return false;
}

static bool is_type_sub_function(ast_t* def, ast_t* fun)
{
  ast_t* members = ast_childidx(def, 4);
  ast_t* member = ast_child(members);

  while(member != NULL)
  {
    if(is_functiondef_sub_function(member, fun))
      return true;

    member = ast_sibling(member);
  }

  return false;
}

static bool is_nominal_sub_structural(ast_t* sub, ast_t* super)
{
  ast_t* def = nominal_def(sub);

  if(def == NULL)
    return false;

  // TODO: reify def with typeargs

  // must be a subtype of every function in super
  ast_t* fun = ast_child(super);

  while(fun != NULL)
  {
    if(!is_type_sub_function(def, fun))
      return false;

    fun = ast_sibling(fun);
  }

  return true;
}

static bool is_nominal_sub_nominal(ast_t* sub, ast_t* super)
{
  ast_t* sub_def = nominal_def(sub);
  ast_t* super_def = nominal_def(super);

  // check we found a definition for both
  if((sub_def == NULL) || (super_def == NULL))
    return false;

  // if we are the same nominal type, our typeargs must be the same
  if(sub_def == super_def)
    return is_eq_typeargs(sub, super);

  // our def might be a TK_TYPEPARAM
  if(ast_id(sub_def) == TK_TYPEPARAM)
  {
    // check if our constraint is a subtype of super
    ast_t* constraint = ast_childidx(sub_def, 1);

    if(ast_id(constraint) == TK_NONE)
      return false;

    return is_typedef_sub_typedef(constraint, ast_parent(super));
  }

  // get our typeparams and typeargs
  ast_t* typeparams = ast_childidx(sub_def, 1);
  ast_t* typeargs = ast_childidx(sub, 1);

  // check traits, depth first
  ast_t* traits = ast_childidx(sub_def, 3);
  ast_t* trait = ast_child(traits);

  while(trait != NULL)
  {
    assert(ast_id(trait) == TK_TYPEDEF);
    bool is_sub;

    if(ast_id(typeparams) == TK_TYPEPARAMS)
    {
      // substitute typeargs in sub for typeparams in trait
      assert(ast_id(typeargs) == TK_TYPEARGS);
      ast_t* r_trait = reify_type(trait, typeparams, typeargs);

      is_sub = is_subtype(r_trait, ast_parent(super));
      ast_free(r_trait);
    } else {
      // no type parameters, use the trait directly
      is_sub = is_subtype(trait, ast_parent(super));
    }

    if(is_sub)
      return true;

    trait = ast_sibling(trait);
  }

  return false;
}

static bool is_union_subtype(ast_t* sub, ast_t* super)
{
  ast_t* left = ast_child(super);
  ast_t* right = ast_sibling(left);
  return is_subtype(sub, left) || is_subtype(sub, right);
}

static bool is_isect_subtype(ast_t* sub, ast_t* super)
{
  ast_t* left = ast_child(super);
  ast_t* right = ast_sibling(left);
  return is_subtype(sub, left) && is_subtype(sub, right);
}

static bool is_nominal_eq_nominal(ast_t* a, ast_t* b)
{
  ast_t* a_def = nominal_def(a);
  ast_t* b_def = nominal_def(b);

  // check we found a definition for both
  if((a_def == NULL) || (b_def == NULL))
    return false;

  // must be the same nominal type
  if(a_def != b_def)
    return false;

  // must have the same typeargs
  return is_eq_typeargs(a, b);
}

static bool is_typedef_sub_typedef(ast_t* sub, ast_t* super)
{
  assert(ast_id(sub) == TK_TYPEDEF);
  assert(ast_id(super) == TK_TYPEDEF);

  super = ast_child(super);

  switch(ast_id(super))
  {
    case TK_UNIONTYPE:
      return is_union_subtype(sub, super);

    case TK_ISECTTYPE:
      return is_isect_subtype(sub, super);

    default: {}
  }

  sub = ast_child(sub);

  switch(ast_id(sub))
  {
    case TK_TUPLETYPE:
    {
      switch(ast_id(super))
      {
        case TK_TUPLETYPE:
        {
          ast_t* left = ast_child(sub);
          ast_t* right = ast_sibling(left);
          ast_t* super_left = ast_child(super);
          ast_t* super_right = ast_sibling(super_left);
          return is_subtype(left, super_left) && is_subtype(right, super_right);
        }

        case TK_NOMINAL:
        case TK_STRUCTURAL:
          return false;

        default: {}
      }
      break;
    }

    case TK_NOMINAL:
    {
      switch(ast_id(super))
      {
        case TK_NOMINAL:
          return is_nominal_sub_nominal(sub, super);

        case TK_STRUCTURAL:
          return is_nominal_sub_structural(sub, super);

        case TK_TUPLETYPE:
          return false;

        default: {}
      }
      break;
    }

    case TK_STRUCTURAL:
    {
      switch(ast_id(super))
      {
        case TK_STRUCTURAL:
          return is_structural_sub_structural(sub, super);

        case TK_TUPLETYPE:
        case TK_NOMINAL:
          return false;

        default: {}
      }
      break;
    }

    default: {}
  }

  assert(0);
  return false;
}

static bool is_typedef_subtype(ast_t* type, ast_t* super)
{
  assert(ast_id(type) == TK_TYPEDEF);
  ast_t* sub = ast_child(type);

  switch(ast_id(sub))
  {
    case TK_UNIONTYPE:
    {
      ast_t* left = ast_child(sub);
      ast_t* right = ast_sibling(left);
      return is_subtype(left, super) && is_subtype(right, super);
    }

    case TK_ISECTTYPE:
    {
      ast_t* left = ast_child(sub);
      ast_t* right = ast_sibling(left);
      return is_subtype(left, super) || is_subtype(right, super);
    }

    case TK_TUPLETYPE:
    case TK_NOMINAL:
    case TK_STRUCTURAL:
    {
      switch(ast_id(super))
      {
        case TK_TYPEDEF:
          return is_typedef_sub_typedef(type, super);

        case TK_TYPEPARAM:
          return is_subtype(type, ast_childidx(super, 1));

        default: {}
      }
      break;
    }

    default: {}
  }

  assert(0);
  return false;
}

static bool is_typedef_eqtype(ast_t* type, ast_t* b)
{
  assert(ast_id(type) == TK_TYPEDEF);
  ast_t* a = ast_child(type);

  switch(ast_id(a))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_STRUCTURAL:
      return is_subtype(a, b) && is_subtype(b, a);

    case TK_TUPLETYPE:
    {
      if(ast_id(b) != TK_TYPEDEF)
        return false;

      b = ast_child(b);

      if(ast_id(b) != TK_TUPLETYPE)
        return false;

      ast_t* a_left = ast_child(a);
      ast_t* a_right = ast_sibling(a_left);
      ast_t* b_left = ast_child(b);
      ast_t* b_right = ast_sibling(b_left);
      return is_eqtype(a_left, b_left) && is_eqtype(a_right, b_right);
    }

    case TK_NOMINAL:
    {
      if(ast_id(b) != TK_TYPEDEF)
        return false;

      b = ast_child(b);

      if(ast_id(b) != TK_NOMINAL)
        return false;

      return is_nominal_eq_nominal(a, b);
    }

    default: {}
  }

  assert(0);
  return false;
}

bool is_subtype(ast_t* sub, ast_t* super)
{
  switch(ast_id(sub))
  {
    case TK_TYPEDEF:
    {
      if(!is_typedef_subtype(sub, super))
        return false;

      // TODO: check cap and ephemeral
      return true;
    }

    case TK_TYPEPARAM:
      return is_subtype(ast_childidx(sub, 1), super);

    default: {}
  }

  assert(0);
  return false;
}

bool is_eqtype(ast_t* a, ast_t* b)
{
  switch(ast_id(a))
  {
    case TK_TYPEDEF:
    {
      if(!is_typedef_eqtype(a, b))
        return false;

      // TODO: check cap and ephemeral
      return true;
    }

    case TK_TYPEPARAM:
    {
      // TODO: is this right?
      return is_eqtype(ast_childidx(a, 1), b);
    }

    default: {}
  }

  assert(0);
  return false;
}