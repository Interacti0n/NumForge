target_sources(numforge_application PRIVATE
    expressions/expression.c
    expressions/expression_domain.c
    expressions/fast_expression.c
    graphs/fast_graph.c
    graphs/graph.c
    graphs/graph_analysis.c
    graphs/graph_projection.c
    equations/equation.c
    equations/equation_format.c
    equations/equation_parse.c
    equations/numeric_root.c
    equations/numeric_root_brent.c
    conversions/conversion.c
    sessions/session.c
    clients/client_store.c
)
