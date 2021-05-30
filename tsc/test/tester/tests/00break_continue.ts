function main() {    
    assert(test_do() == 5, "failed. 1");
    assert(test_while() == 5, "failed. 2");
    assert(test_for() == 4, "failed. 3");
    test_for_empty();

    print("done.");
}


function test_do()
{
	let i = 0;
	do
	{
		if (i == 5)
			break;

		if (i == 2)
			continue;

		print("i = ", i);

		let j = 0;
		do
		{
			if (j == 3)
				break;

			if (j == 2)
				continue;

			print("j = ", j);
		}
		while (j++ < 5)
	}
	while (i++ < 10)

    return i;
}

function test_while()
{
	let i = 0;
	while (i++ < 10)
	{
		if (i == 5)
			break;

		if (i == 2)
			continue;

		print("i = ", i);

		let j = 0;
		while (j++ < 5)
		{
			if (j == 3)
				break;

			if (j == 2)
				continue;

			print("j = ", j);
		}
	}

    return i;
}

function test_for()
{
    let j = 0;
	for (let i = 0; i < 10; i++)
	{
		if (i == 5)
			break;

		if (i == 2)
			continue;

		print("i = ", i);

		for (let j = 0; j < 5; j++)
		{
			if (j == 3)
				break;

			if (j == 2)
				continue;

			print("j = ", j);
		}

        j = i;
	}

    return j;
}

function test_for_empty()
{
	for ( ; ; )
	{
        break;
	}    
}
